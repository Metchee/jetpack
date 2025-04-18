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

Server::Server(int argc, char* argv[]) : _packetsUpdated(false), _serverFd(-1), _nbClients(1)
{
    config.parseArgs(argc, argv);
    config.validate();
    config.loadMap();
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

    auto last_broadcast = std::chrono::steady_clock::now();

    while (true) {
        poll_fds.resize(1);
        for (auto& client : _fdsList) {
            pollfd client_pollfd;
            client_pollfd.fd = client->fd;
            client_pollfd.events = POLLIN;
            poll_fds.push_back(client_pollfd);
        }
        int ready = poll(poll_fds.data(), poll_fds.size(), -1);
        if (ready < 0) {
            if (config.debug_mode) {
                std::cerr << "[SERVER] Poll error: " << strerror(errno) << std::endl;
            }
            continue;
        }
        for (size_t i = 0; i < poll_fds.size(); ++i) {
            if (poll_fds[i].revents & POLLIN) {
                if (poll_fds[i].fd == _serverFd) {
                    handleNewConnection();
                } else {
                    handleClientData(poll_fds[i].fd);
                }
            }
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_broadcast).count();
        if (_packetsUpdated && elapsed >= 50) {
            broadcastPackets();
            last_broadcast = now;
        }
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
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

    int client_id = _nbClients++;
    auto new_pollfd = std::make_shared<pollfd>();
    new_pollfd->fd = client_fd;
    new_pollfd->events = POLLIN;
    _fdsList.push_back(new_pollfd);
    _clientIds[client_fd] = client_id;

    PacketModule welcomePacket(_nbClients);
    auto& pkt = welcomePacket.getPacket();
    pkt.nb_client = _nbClients;
    pkt.client_id = client_id;
    pkt.playerState[client_id] = PacketModule::WAITING;
    pkt.playerPosition[client_id] = std::make_pair(100, 300);
    std::ifstream mapFile(config.map_file);
    mapFile.seekg(0, std::ios::end);
    std::streamsize file_size = mapFile.tellg();
    mapFile.seekg(0, std::ios::beg);
    mapFile.read(pkt.map, file_size);
    pkt.map[file_size] = '\0';

    sendPacket(client_fd, welcomePacket);
    _packets.emplace(client_id, welcomePacket);

    if (config.debug_mode) {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, INET_ADDRSTRLEN);
        std::cout << "[SERVER] New client " << client_id << " from " << ip << std::endl;
    }

    for (auto& packetPair : _packets) {
        packetPair.second.getPacket().nb_client = _nbClients;
    }
    _packetsUpdated = true;
    broadcastPackets();

    if (_nbClients >= 2) {
        for (auto& clientPair : _clientIds) {
            int clientId = clientPair.second;
            _packets[clientId].getPacket().playerState[clientId] = PacketModule::PLAYING;
        }
        _packetsUpdated = true;
        broadcastPackets();

        if (config.debug_mode) {
            std::cout << "[SERVER] Game started with " << (_nbClients - 1) << " players" << std::endl;
        }
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
    PacketModule pkt(_nbClients);
    if (!readPacket(client_fd, pkt)) {
        removeClient(client_fd);
        return;
    }
    if (config.debug_mode) {
        std::cout << "[SERVER] Successfully received packet from client " << it->second << std::endl;
    }
    _packets.insert_or_assign(it->second, pkt);
    _packetsUpdated = true;
    // broadcastPackets();
}

void Server::removeClient(int client_fd)
{
    int client_id = -1;
    auto id_it = _clientIds.find(client_fd);

    if (id_it != _clientIds.end()) {
        client_id = id_it->second;
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
        std::cout << "[SERVER] Client disconnected (fd: " << client_fd << ")" << std::endl;
    }
    _nbClients--;
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
        // packetModule.display("[SERVER]");
    }
    return bytes_sent;
}

bool Server::readPacket(int client_fd, PacketModule &packetModule)
{
    if (config.debug_mode) {
        std::cout << "[SERVER] Reading packet from client " << client_fd << std::endl;
    }
    auto& pkt = packetModule.getPacket();
    ssize_t bytes_read = recv(client_fd, &pkt, sizeof(pkt), MSG_WAITALL);

    if (bytes_read <= 0) {
        if (config.debug_mode) {
            std::cerr << "[SERVER] Failed to read packet from client " << client_fd << std::endl;
        }
        return false;
    }
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
    if (!_packetsUpdated)
        return;

    for (auto& client : _fdsList) {
        if (client->fd != _serverFd) {
            auto it = _clientIds.find(client->fd);
            if (it != _clientIds.end()) {
                sendPacket(client->fd, _packets[it->second]);
            }
        }
    }
    _packetsUpdated = false;
}
