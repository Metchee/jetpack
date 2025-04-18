#include "../shared_include/Server.hpp"
#include "../shared_include/Packet.hpp"
#include <iostream>
#include <fstream>
#include <sys/cdefs.h>
#include <sys/poll.h>
#include <unistd.h>
#include <getopt.h>
#include <memory>
#include <vector>
#include <arpa/inet.h>
#include <unordered_map>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <chrono>
#include <mutex>

Server::Server(int argc, char* argv[]) : _packetsUpdated(false), _serverFd(-1), _nbClients(1), _running(true)
{
    // Parse command line arguments
    config.parseArgs(argc, argv);
    config.validate();
    config.loadMap();

    // Initialize server socket
    _serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (_serverFd < 0) {
        throw std::runtime_error(std::string("Failed to create socket: ") + strerror(errno));
    }
    int opt = 1;
    if (setsockopt(_serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        throw std::runtime_error(std::string("Failed to set socket options: ") + strerror(errno));
    }
    // Bind socket to address
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(config.port);
    if (bind(_serverFd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        throw std::runtime_error(std::string("Failed to bind socket: ") + strerror(errno));
    }

    // Set socket to listen for incoming connections
    if (listen(_serverFd, MAX_CLIENTS) < 0) {
        throw std::runtime_error(std::string("Failed to listen on socket: ") + strerror(errno));
    }
    if (config.debug_mode) {
        std::cout << "[SERVER] Server started on port " << config.port << std::endl;
    }
}

Server::~Server()
{
    stop();
}

void Server::run()
{
    // set up poll
    std::vector<pollfd> poll_fds;
    pollfd server_pollfd;
    server_pollfd.fd = _serverFd;
    server_pollfd.events = POLLIN;
    poll_fds.push_back(server_pollfd);

    // set clock for broadcast
    auto lastBroadcast = std::chrono::steady_clock::now();
    
    while (_running) {
        poll_fds.resize(1); 
        {
            // lock the mutex to access the client list
            std::lock_guard<std::mutex> lock(_clientsMutex);
            for (auto& client : _fdsList) {
                pollfd client_pollfd;
                client_pollfd.fd = client->fd;
                client_pollfd.events = POLLIN;
                poll_fds.push_back(client_pollfd);
            }
        }

        // poll for events
        int ready = poll(poll_fds.data(), poll_fds.size(), 100); // 100ms timeout
        if (ready < 0) {
            if (errno == EINTR) {
                continue;   
            }
            if (config.debug_mode) {
                std::cerr << "[SERVER] Poll error: " << strerror(errno) << std::endl;
            }
            break;
        }
        // check for new connections or data from clients
        for (size_t i = 0; i < poll_fds.size(); ++i) {
            if (!_running)
                break;
            if (poll_fds[i].revents & POLLIN) {
                if (poll_fds[i].fd == _serverFd) {
                    handleNewConnection();
                } else {
                    handleClientData(poll_fds[i].fd);
                }
            }
        }
        // broadcast packets to clients
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastBroadcast).count() >= 33) { // ~30fps
            broadcastPackets();
            lastBroadcast = now;
        }
    }
}

void Server::stop()
{
    _running = false;
    // close all client sockets
    std::lock_guard<std::mutex> lock(_clientsMutex);
    for (auto& client : _fdsList) {
        shutdown(client->fd, SHUT_RDWR);
        close(client->fd);
    }
    // clear local variables
    _fdsList.clear();
    _clientIds.clear();
    _packets.clear();

    // close server socket
    if (_serverFd >= 0) {
        shutdown(_serverFd, SHUT_RDWR);
        close(_serverFd);
        _serverFd = -1;
    }
    if (config.debug_mode) {
        std::cout << "[SERVER] Server stopped" << std::endl;
    }
}

void Server::handleNewConnection()
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Accept new connection
    int client_fd = accept(_serverFd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        if (config.debug_mode) {
            std::cerr << "[SERVER] Failed to accept connection: " << strerror(errno) << std::endl;
        }
        return;
    }

    // Set the client socket to non-blocking mode
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

    int client_id = _nbClients++;
    auto new_pollfd = std::make_shared<pollfd>();
    new_pollfd->fd = client_fd;
    new_pollfd->events = POLLIN;
    {
        std::lock_guard<std::mutex> lock(_clientsMutex);
        _fdsList.push_back(new_pollfd);
        _clientIds[client_fd] = client_id;
    }

    // Create a packet for the new client
    PacketModule welcomePacket(_nbClients);
    auto& pkt = welcomePacket.getPacket();
    pkt.nb_client = _nbClients;
    pkt.client_id = client_id;
    pkt.playerState[client_id] = PacketModule::WAITING;
    pkt.playerPosition[client_id] = std::make_pair(100, 600 / 2);

    // Load the map file into the packet
    std::ifstream mapFile(config.map_file);
    if (!mapFile) {
        throw std::runtime_error("Failed to open map file: " + config.map_file);
    }
    mapFile.seekg(0, std::ios::end);
    std::streamsize file_size = mapFile.tellg();
    mapFile.seekg(0, std::ios::beg);
    if (file_size >= MAP_SIZE) {
        throw std::runtime_error("Map file too large (max " + std::to_string(MAP_SIZE) + " bytes)");
    }
    mapFile.read(pkt.map, file_size);
    pkt.map[file_size] = '\0';

    // Update game state based on number of clients
    if (_nbClients >= 2) {
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            pkt.playerState[i] = PacketModule::PLAYING;
        }
    }

    // Send welcome packet to the new client
    if (sendPacket(client_fd, welcomePacket) < 0) {
        removeClient(client_fd);
        return;
    }

    // Store the packet and mark packets as updated
    {
        std::lock_guard<std::mutex> lock(_clientsMutex);
        _packets.emplace(client_id, welcomePacket);
        _packetsUpdated = true;
    }

    if (config.debug_mode) {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, INET_ADDRSTRLEN);
        std::cout << "[SERVER] New client " << client_id << " from " << ip << std::endl;
    }

    // Broadcast updated state to all clients
    broadcastPackets();
}

void Server::handleClientData(int client_fd)
{
    if (config.debug_mode) {
        std::cout << "[SERVER] Handling data from client " << client_fd << std::endl;
    }

    // check if client is still connected
    auto it = _clientIds.find(client_fd);
    if (it == _clientIds.end()) {
        removeClient(client_fd);
        return;
    }

    // read packet from client
    PacketModule pkt(_nbClients);
    if (!readPacket(client_fd, pkt)) {
        removeClient(client_fd);
        return;
    }
    if (config.debug_mode) {
        std::cout << "[SERVER] Successfully received packet from client " << it->second << std::endl;
    }

    // update packet with client ID
    _packets.insert_or_assign(it->second, pkt);
    _packetsUpdated = true;
}

void Server::removeClient(int client_fd)
{
    // remove client from idsList
    int client_id = -1;
    auto id_it = _clientIds.find(client_fd);

    // if client ID exists, remove it from the map
    if (id_it != _clientIds.end()) {
        client_id = id_it->second;
    }

    // remove client from fdsList
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
        std::cout << "[SERVER] Client disconnected (fd: " << client_fd << ")" << std::endl;
    }

    // decrement the number of clients
    _nbClients--;
}

int Server::sendPacket(int client_fd, PacketModule &packetModule)
{
    // create packet to send
    const auto& pkt = packetModule.getPacket();
    int bytes_sent = send(client_fd, &pkt, sizeof(pkt), 0);

    if (bytes_sent < 0) {
        if (config.debug_mode) {
            std::cerr << "[SERVER] Failed to send packet to client " << client_fd << std::endl;
        }
        removeClient(client_fd);
    }
    if (config.debug_mode) {
    }
    return bytes_sent;
}

bool Server::readPacket(int client_fd, PacketModule &packetModule)
{
    if (config.debug_mode) {
        std::cout << "[SERVER] Reading packet from client " << client_fd << std::endl;
    }
    // create packet to receive
    auto& pkt = packetModule.getPacket();
    ssize_t bytes_read = recv(client_fd, &pkt, sizeof(pkt), MSG_WAITALL);

    if (bytes_read <= 0) {
        if (config.debug_mode) {
            std::cerr << "[SERVER] Failed to read packet from client " << client_fd << std::endl;
        }
        return false;
    }
    // check if the entire packet was received
    if (static_cast<size_t>(bytes_read) != sizeof(pkt)) {
        if (config.debug_mode) {
            std::cerr << "[SERVER] Incomplete packet received from client " << client_fd << std::endl;
        }
        return false;
    }
    if (config.debug_mode) {
        std::cout << "\n[SERVER] Received packet from client " << client_fd << std::endl;
    }
    return true;
}

void Server::broadcastPackets()
{
    // Consolidate all client data into a single packet
    PacketModule broadcastPacket(_nbClients);
    auto& pkt = broadcastPacket.getPacket();
    pkt.nb_client = _nbClients;

    // Update game state based on number of clients
    if (_nbClients >= 2) {
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            pkt.playerState[i] = PacketModule::PLAYING;
        }
    } else {
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            pkt.playerState[i] = PacketModule::WAITING;
        }
    }

    // Copy map data (assuming it's the same for all clients)
    std::ifstream mapFile(config.map_file);
    if (mapFile) {
        mapFile.seekg(0, std::ios::end);
        std::streamsize file_size = mapFile.tellg();
        mapFile.seekg(0, std::ios::beg);
        if (file_size < MAP_SIZE) {
            mapFile.read(pkt.map, file_size);
            pkt.map[file_size] = '\0';
        }
    }

    // Update player positions and states from _packets
    {
        std::lock_guard<std::mutex> lock(_clientsMutex);
        for (const auto& [client_id, packet] : _packets) {
            pkt.playerPosition[client_id] = packet.getPosition();
            pkt.playerState[client_id] = packet.getstate();
        }
    }

    // Send the consolidated packet to all clients
    for (auto& client : _fdsList) {
        if (client->fd != _serverFd) {
            auto it = _clientIds.find(client->fd);
            if (it != _clientIds.end()) {
                pkt.client_id = it->second; // Set the client_id for the receiving client
                if (sendPacket(client->fd, broadcastPacket) < 0) {
                    removeClient(client->fd);
                } else if (config.debug_mode) {
                    std::cout << "[SERVER] Broadcast packet to client " << it->second << std::endl;
                    broadcastPacket.display("[SERVER] Broadcast: ");
                }
            }
        }
    }
    _packetsUpdated = false;
}