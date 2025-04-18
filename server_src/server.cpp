#include "../shared_include/Server.hpp"
#include "../shared_include/Game.hpp"
#include <iostream>
#include <fstream>
#include <sys/poll.h>
#include <unistd.h>
#include <memory>
#include <vector>
#include <arpa/inet.h>
#include <fcntl.h>
#include <chrono>
#include <thread>

// Update constructor to use const char* argv[]
Server::Server(int argc, const char* argv[]) : 
    _packetsUpdated(false), 
    _serverFd(-1), 
    _nbClients(0),
    _game(std::make_unique<Game>()),
    _lastUpdateTime(std::chrono::steady_clock::now())
{
    // Convert to non-const for parseArgs which expects char*[]
    char** non_const_argv = const_cast<char**>(argv);
    config.parseArgs(argc, non_const_argv);
    
    // Load map
    if (!_game->loadMap(config.map_file)) {
        throw std::runtime_error("Failed to load map: " + config.map_file);
    }
    
    // Setup server socket
    _serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (_serverFd < 0) {
        throw std::runtime_error("Failed to create socket");
    }
    
    int opt = 1;
    if (setsockopt(_serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        throw std::runtime_error("Failed to set socket options");
    }
    
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(config.port);
    
    if (bind(_serverFd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        throw std::runtime_error("Failed to bind socket");
    }
    
    if (listen(_serverFd, 10) < 0) {
        throw std::runtime_error("Failed to listen on socket");
    }
    
    if (config.debug_mode) {
        std::cout << "[SERVER] Server started on port " << config.port << std::endl;
        std::cout << "[SERVER] Loaded map: " << config.map_file << std::endl;
    }
}
void Server::stop()
{
    for (auto& client : _fdsList) {
        close(client->fd);
    }
    _fdsList.clear();
    _clientIds.clear();
    _packets.clear();
    if (_serverFd >= 0) {
        close(_serverFd);
        _serverFd = -1;
    }
    if (config.debug_mode) {
        std::cout << "[SERVER] Server stopped" << std::endl;
    }
}

Server::~Server()
{
    stop();
}

void Server::run()
{
    std::vector<pollfd> poll_fds;
    pollfd server_pollfd;
    server_pollfd.fd = _serverFd;
    server_pollfd.events = POLLIN;
    poll_fds.push_back(server_pollfd);
    
    while (true) {
        // Reset poll_fds with current connections
        poll_fds.resize(1); 
        for (auto& client : _fdsList) {
            pollfd client_pollfd;
            client_pollfd.fd = client->fd;
            client_pollfd.events = POLLIN;
            poll_fds.push_back(client_pollfd);
        }
        
        // Poll with short timeout to allow game updates
        int ready = poll(poll_fds.data(), poll_fds.size(), 16); // 16ms timeout (~60fps)
        
        if (ready < 0) {
            if (config.debug_mode) {
                std::cerr << "[SERVER] Poll error: " << strerror(errno) << std::endl;
            }
            continue;
        }
        
        // Handle ready file descriptors
        for (size_t i = 0; i < poll_fds.size(); ++i) {
            if (poll_fds[i].revents & POLLIN) {
                if (poll_fds[i].fd == _serverFd) {
                    handleNewConnection();
                } else {
                    handleClientData(poll_fds[i].fd);
                }
            }
        }
        
        // Update game state
        updateGame();
        
        // Broadcast updated state to all clients
        broadcastGameState();
        
        // Check if game should start
        if (_clientIds.size() >= 2 && !_game->isGameStarted() && !_game->isGameOver()) {
            _game->startGame();
            if (config.debug_mode) {
                std::cout << "[SERVER] Game started with " << _clientIds.size() << " players" << std::endl;
            }
        }
    }
}

void Server::updateGame()
{
    auto currentTime = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(currentTime - _lastUpdateTime).count();
    _lastUpdateTime = currentTime;
    
    _game->update(deltaTime);
    
    // Update packet data from game state
    for (auto& [id, packet] : _packets) {
        _game->fillPacket(packet);
    }
    
    _packetsUpdated = true;
}

void Server::handleNewConnection()
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(_serverFd, (struct sockaddr*)&client_addr, &client_len);
    
    if (client_fd < 0) {
        if (config.debug_mode) {
            std::cerr << "[SERVER] Failed to accept connection" << std::endl;
        }
        return;
    }
    
    // Set non-blocking
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
    
    // Assign client ID
    int client_id = _nbClients++;
    
    // Add to connection list
    auto new_pollfd = std::make_shared<pollfd>();
    new_pollfd->fd = client_fd;
    new_pollfd->events = POLLIN;
    _fdsList.push_back(new_pollfd);
    _clientIds[client_fd] = client_id;
    
    // Add player to game
    _game->addPlayer(client_id);
    
    // Create initial packet
    PacketModule welcomePacket;
    auto& pkt = welcomePacket.getPacket();
    pkt.nb_client = _clientIds.size();
    pkt.client_id = client_id;
    
    // Send welcome packet with map data
    sendPacket(client_fd, welcomePacket);
    
    // Send map data
    sendMapData(client_fd);
    
    _packets.emplace(client_id, welcomePacket);
    
    if (config.debug_mode) {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, INET_ADDRSTRLEN);
        std::cout << "[SERVER] New client " << client_id << " from " << ip << std::endl;
    }
}

void Server::handleClientData(int client_fd)
{
    if (config.debug_mode) {
        std::cout << "[SERVER] Handling data from client " << client_fd << std::endl;
    }
    
    auto it = _clientIds.find(client_fd);
    if (it == _clientIds.end()) {
        removeClient(client_fd);
        return;
    }
    
    PacketModule pkt;
    if (!readPacket(client_fd, pkt)) {
        removeClient(client_fd);
        return;
    }
    
    int client_id = it->second;
    
    // Extract player input from packet
    const auto& packet = pkt.getPacket();
    
    // Process player input and update game state
    Player* player = _game->getPlayer(client_id);
    if (player) {
        // Determine player direction based on packet data
        Direction playerDir = Direction::NONE;
        
        // Extract direction from packet (this depends on your protocol)
        // For simplicity, we'll use the position delta to infer direction
        std::pair<int, int> packetPos = packet.playerPosition[packet.client_id];
        std::pair<int, int> currentPos = player->getPosition();
        
        if (packetPos.second < currentPos.second) {
            playerDir = Direction::UP;
        } else if (packetPos.second > currentPos.second) {
            playerDir = Direction::DOWN;
        } else if (packetPos.first < currentPos.first) {
            playerDir = Direction::LEFT;
        } else if (packetPos.first > currentPos.first) {
            playerDir = Direction::RIGHT;
        }
        
        _game->handlePlayerInput(client_id, playerDir);
    }
    
    // Update our packet record
    _packets[client_id] = pkt;
    
    if (config.debug_mode) {
        std::cout << "[SERVER] Successfully processed input from client " << client_id << std::endl;
    }
}

void Server::removeClient(int client_fd)
{
    int client_id = -1;
    auto id_it = _clientIds.find(client_fd);

    if (id_it != _clientIds.end()) {
        client_id = id_it->second;
        // Remove player from game
        if (_game) {
            _game->removePlayer(client_id);
        }
    }
    
    for (auto it = _fdsList.begin(); it != _fdsList.end(); ++it) {
        if ((*it)->fd == client_fd) {
            _fdsList.erase(it);
            break;
        }
    }
    
    _clientIds.erase(client_fd);
    if (client_id != -1) {
        _packets.erase(client_id);
    }
    
    close(client_fd);
    
    if (config.debug_mode) {
        std::cout << "[SERVER] Client disconnected (fd: " << client_fd << ", id: " << client_id << ")" << std::endl;
    }
}

void Server::broadcastGameState()
{
    if (!_packetsUpdated) {
        return;
    }
    
    // Update all packets with current game state
    for (auto& [client_id, packet] : _packets) {
        _game->fillPacket(packet);
    }
    
    // Send updated packets to all clients
    for (const auto& client : _fdsList) {
        auto it = _clientIds.find(client->fd);
        if (it != _clientIds.end()) {
            int client_id = it->second;
            if (_packets.find(client_id) != _packets.end()) {
                sendPacket(client->fd, _packets[client_id]);
            }
        }
    }
    
    _packetsUpdated = false;
}

int Server::sendPacket(int client_fd, PacketModule &packetModule)
{
    const auto& pkt = packetModule.getPacket();
    int bytes_sent = send(client_fd, &pkt, sizeof(pkt), 0);

    if (bytes_sent < 0) {
        if (config.debug_mode) {
            std::cerr << "[SERVER] Failed to send packet to client " << client_fd << std::endl;
        }
        removeClient(client_fd);
    }
    
    if (config.debug_mode) {
        packetModule.display("[SERVER] Sent packet: ");
    }
    
    return bytes_sent;
}

bool Server::readPacket(int client_fd, PacketModule &packetModule)
{
    auto& pkt = packetModule.getPacket();
    ssize_t bytes_read = recv(client_fd, &pkt, sizeof(pkt), MSG_DONTWAIT);

    if (bytes_read <= 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            if (config.debug_mode) {
                std::cerr << "[SERVER] Failed to read packet from client " << client_fd << std::endl;
            }
            return false;
        }
        return true; // No data available, not an error
    }
    
    if (static_cast<size_t>(bytes_read) != sizeof(pkt)) {
        if (config.debug_mode) {
            std::cerr << "[SERVER] Incomplete packet received from client " << client_fd << std::endl;
        }
        return false;
    }
    
    if (config.debug_mode) {
        packetModule.display("[SERVER] Received packet: ");
    }
    
    return true;
}

void Server::sendMapData(int client_fd)
{
    std::string mapData = _game->getMap().serialize();
    
    // Send map size first
    uint32_t mapSize = mapData.size();
    send(client_fd, &mapSize, sizeof(mapSize), 0);
    
    // Send map data
    send(client_fd, mapData.c_str(), mapData.size(), 0);
    
    if (config.debug_mode) {
        std::cout << "[SERVER] Sent map data to client " << client_fd << " (size: " << mapSize << " bytes)" << std::endl;
    }
}