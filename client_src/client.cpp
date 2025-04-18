#include "../shared_include/Client.hpp"
#include <cstdio>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <fcntl.h>
#include <errno.h>

std::atomic<bool> g_shutdown{false};

void signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    g_shutdown = true;
}

void ClientModule::Client::parseArguments(int argc, const char *argv[]) {
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
    if (serverIp.empty() || serverPort <= 0) {
        throw std::runtime_error("Missing required arguments. Usage: ./jetpack_client -h <ip> -p <port> [-d]");
    }
}

ClientModule::Client::Client(int ac, const char *av[]) :
    fd(-1), id(-1), serverPort(-1), serverIp(""), connected(false), debugMode(false)
{
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    parseArguments(ac, av);
    if (debugMode) {
        std::cout << "[CLIENT] Debug mode enabled" << std::endl;
        std::cout << "[CLIENT] Server IP: " << serverIp << std::endl;
        std::cout << "[CLIENT] Server Port: " << serverPort << std::endl;
    }
}

ClientModule::Client::~Client() {
    if (connected) {
        close(fd);
    }
    stop();
    if (debugMode) {
        std::cout << "[CLIENT] Destroyed client" << std::endl;
    }
}

void ClientModule::Client::run() {
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
        std::cout << "[CLIENT] Connected to server at " << getAddress() << std::endl;
    }
    startThread();
    runThread();
}

void ClientModule::Client::stop() {
    if (connected) {
        connected = false;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        close(fd);
        fd = -1;
        if (debugMode) {
            std::cout << "[CLIENT] Disconnected from server" << std::endl;
        }
    }
}

std::string ClientModule::Client::getAddress() const {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &address.sin_addr, ip, INET_ADDRSTRLEN);
    return std::string(ip) + ":" + std::to_string(ntohs(address.sin_port));
}

void ClientModule::Client::networkThread()
{
    if (debugMode) {
        std::cout << "[CLIENT] Network thread started" << std::endl;
    }
    PacketModule incomingPacket;
    PacketModule outgoingPacket;
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    
    while (connected && !g_shutdown) {
        // Receive packet
        ssize_t result = recv(fd, &incomingPacket, sizeof(PacketModule::Packet), MSG_DONTWAIT);
        if (result > 0) {
            if (static_cast<size_t>(result) == sizeof(PacketModule::Packet)) {
                if (debugMode) {
                    std::lock_guard<std::mutex> lock(_packetMutex);
                    std::cout << "[CLIENT] Received packet from server" << std::endl;
                    incomingPacket.display("[CLIENT] Received: ");
                }
                {
                    std::lock_guard<std::mutex> lock(_packetMutex);
                    if (id == -1) {
                        id = incomingPacket.getClientId();
                        if (debugMode) {
                            std::cout << "[CLIENT] Assigned client ID: " << id << std::endl;
                        }
                    }
                    packet = incomingPacket; // Update the shared packet
                }
            } else {
                if (debugMode) {
                    std::cerr << "[CLIENT] Incomplete packet received: " << result << " bytes" << std::endl;
                }
            }
        } else if (result == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                if (debugMode) {
                    std::cerr << "[CLIENT] Network error: " << strerror(errno) << std::endl;
                }
                connected = false;
                break;
            }
        } else if (result == 0) {
            if (debugMode) {
                std::cout << "[CLIENT] Server closed connection" << std::endl;
            }
            connected = false;
            break;
        }
        
        // Send updated packet back to server
        {
            std::lock_guard<std::mutex> lock(_packetMutex);
            outgoingPacket = packet;
        }
        result = send(fd, &outgoingPacket, sizeof(PacketModule::Packet), MSG_DONTWAIT);
        if (result == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                if (debugMode) {
                    std::cerr << "[CLIENT] Send error: " << strerror(errno) << std::endl;
                }
                connected = false;
                break;
            }
        } else if (result > 0 && debugMode) {
            std::lock_guard<std::mutex> lock(_packetMutex);
            outgoingPacket.display("[CLIENT] Sent: ");
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    if (debugMode) {
        std::cout << "[CLIENT] Network thread stopped" << std::endl;
    }
}
void ClientModule::Client::startThread() {
    if (debugMode) {
        std::cout << "[CLIENT] Starting threads" << std::endl;
    }
    try {
        _networkThread = std::thread(&Client::networkThread, this);
        _gameThread = std::thread(&Client::gameThread, this);
    } catch (const std::system_error& e) {
        std::cerr << "[CLIENT] Thread start failed: " << e.what() << std::endl;
        connected = false;
        throw;
    }
}

void ClientModule::Client::runThread() {
    if (debugMode) {
        std::cout << "[CLIENT] Waiting for threads to finish" << std::endl;
    }
    if (_networkThread.joinable()) {
        _networkThread.join();
    }
    if (_gameThread.joinable()) {
        _gameThread.join();
    }
}