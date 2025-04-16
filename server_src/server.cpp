#include "../include/Server.hpp"
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

Server::Server(int argc, char* argv[]) : _nbClients(1)
{
    config.parseArgs(argc, argv);
    loadMap();
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
    if (bind(_serverFd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        throw std::runtime_error("Failed to bind socket");
    }
    if (listen(_serverFd, 10) < 0) {
        throw std::runtime_error("Failed to listen on socket");
    }
    auto server_pollfd = std::make_shared<pollfd>();
    server_pollfd->fd = _serverFd;
    server_pollfd->events = POLLIN;
    _fdsList.push_back(server_pollfd);
    if (config.debug_mode) {
        std::cout << "[SERVER] Server started on port " << config.port << std::endl;
        std::cout << "[SERVER] Debug mode enabled" << std::endl;
        std::cout << "[SERVER] Waiting for connections..." << std::endl;
    }
}

Server::~Server()
{
    stop();
}

void Server::loadMap()
{
    std::ifstream file(config.map_file);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open map file: " + config.map_file);
    }
    if (config.debug_mode) {
        std::cout << "[SERVER] Loaded map: " << config.map_file << std::endl;
    }
}

void Server::run()
{
    bool running = true;
    int status;
    while (running) {
        std::vector<pollfd> poll_fds;
        for (const auto& fd : _fdsList) {
            poll_fds.push_back(*fd);
        }
        status = poll(poll_fds.data(), poll_fds.size(), -1);
        if (status < 0) {
            if (config.debug_mode) {
                std::cerr << "[SERVER] Poll error" << std::endl;
            }
            continue;
        }
        for (size_t i = 0; i < poll_fds.size(); i++) {
            if (poll_fds[i].revents & POLLIN) {
                if (poll_fds[i].fd == _serverFd) {
                    handleNewConnection();
                } else {
                    handleClientData(poll_fds[i].fd);
                }
            }
        }
    }
}

void Server::stop()
{
    for (auto& client : _fdsList) {
        close(client->fd);
        if (config.debug_mode) {
            std::cout << "[SERVER] Forcefully closed connection with client " << client->fd << std::endl;
        }
    }
    _packets.clear();
    _fdsList.clear();
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
    if (config.debug_mode) {
        std::cout << "[SERVER] Accepting new connection..." << std::endl;
    }
    int client_fd = accept(_serverFd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        if (config.debug_mode) {
            std::cerr << "[SERVER] Failed to accept new connection" << std::endl;
        }
        return;
    }
    int client_id = _nbClients++;
    if (config.debug_mode) {
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        std::cout << "[SERVER] New connection from " << client_ip 
                  << ":" << ntohs(client_addr.sin_port) 
                  << " assigned ID: " << client_id 
                  << " (fd: " << client_fd << ")" << std::endl;
    }
    auto new_pollfd = std::make_shared<pollfd>();
    new_pollfd->fd = client_fd;
    new_pollfd->events = POLLIN;
    _fdsList.push_back(new_pollfd);
    std::string welcomeMsg = "Welcome! Your client ID is: " + std::to_string(client_id) + "\n";
    if (send(client_fd, welcomeMsg.c_str(), welcomeMsg.size(), 0) < 0) {
        if (config.debug_mode) {
            std::cerr << "[SERVER] Failed to send welcome message to client " << client_id << std::endl;
        }
    }
}

void Server::handleClientData(int client_fd)
{
    char buffer[1024];
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0) {
        if (bytes_read == 0 && config.debug_mode) {
            std::cout << "[SERVER] Client (fd: " << client_fd << ") disconnected" << std::endl;
        } else if (config.debug_mode) {
            std::cerr << "[SERVER] Error reading from client (fd: " << client_fd << ")" << std::endl;
        }
        close(client_fd);
        removeClient(client_fd);
        return;
    }
    buffer[bytes_read] = '\0';
    if (config.debug_mode) {
        std::cout << "[SERVER] Received from client (fd: " << client_fd << "): " << buffer;
    }
    std::string response = "ACK (fd: " + std::to_string(client_fd) + "): " + buffer;
    if (send(client_fd, response.c_str(), response.size(), 0) < 0) {
        if (config.debug_mode) {
            std::cerr << "[SERVER] Failed to send response to client (fd: " << client_fd << ")" << std::endl;
        }
    }
}

void Server::removeClient(int client_fd)
{
    for (auto it = _fdsList.begin(); it != _fdsList.end(); ++it) {
        if ((*it)->fd == client_fd) {
            _fdsList.erase(it);
            break;
        }
    }
    close(client_fd);
}
