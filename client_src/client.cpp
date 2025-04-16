#include "../include/Client.hpp"
#include <cstdio>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <iostream>

void Client::parseArguments(int argc, const char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" && i + 1 < argc) {
            serverIp = argv[++i];
        } else if (arg == "-p" && i + 1 < argc) {
            serverPort = std::stoi(argv[++i]);
        } else if (arg == "-d") {
            debugMode = true;
        }
    }
}

Client::Client(int ac, const char *av[]) :
    fd(-1), id(-1), serverPort(-1), serverIp(""), connected(false)
{
    parseArguments(ac, av);
}

Client::~Client() {
    if (connected) {
        close(fd);
    }
}
void Client::run()
{
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throw std::runtime_error("Failed to create socket");
    }
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    if (inet_pton(AF_INET, serverIp.c_str(), &serverAddr.sin_addr) <= 0) {
        throw std::runtime_error("Invalid server address");
    }
    if (connect(fd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        throw std::runtime_error("Connection failed");
    }
    connected = true;
    address = serverAddr;
    if (debugMode) {
        std::cout << "Connected to server at " << getAddress() << std::endl;
    }
    std::string message = "Hello from client!";
    std::vector<char> data(message.begin(), message.end());
    send(data);

    while (connected) {
        std::vector<char> data = receive(MAX_INPUT);
        if (data.empty()) {
            break;
        }
        if (debugMode) {
            std::cout << "Received: " << std::string(data.begin(), data.end()) << std::endl;
        }
    }
}

void Client::stop() {
    connected = false;
}

void Client::send(const std::vector<char>& data)
{
    if (!connected)
        throw std::runtime_error("Not connected");
    ssize_t sent = ::send(fd, data.data(), data.size(), 0);
    if (sent < 0) {
        connected = false;
        throw std::runtime_error("Send failed");
    }
}

std::vector<char> Client::receive(size_t maxSize)
{
    if (!connected)
        throw std::runtime_error("Not connected");
    std::vector<char> buffer(maxSize);
    ssize_t received = recv(fd, buffer.data(), buffer.size(), 0);
    if (received <= 0) {
        connected = false;
        return {};
    }
    buffer.resize(received);
    for (size_t i = 0; i < buffer.size(); i++) {
        printf("%c", buffer[i]);
    }
    return buffer;
}

std::string Client::getAddress() const {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &address.sin_addr, ip, INET_ADDRSTRLEN);
    return std::string(ip) + ":" + std::to_string(ntohs(address.sin_port));
}

int Client::getFd() const {return fd;}
int Client::getId() const {return id;}
void Client::setId(int id) {this->id = id;}
bool Client::isConnected() const {return connected;}
