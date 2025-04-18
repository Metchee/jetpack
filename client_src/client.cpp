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

// Global signal handler for cleaner shutdown
std::atomic<bool> g_shutdown{false};

void signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    g_shutdown = true;
}

// Parse command line arguments
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
    
    // Check if required arguments are provided
    if (serverIp.empty() || serverPort <= 0) {
        throw std::runtime_error("Missing required arguments. Usage: ./jetpack_client -h <ip> -p <port> [-d]");
    }
}

// Constructor
ClientModule::Client::Client(int ac, const char *av[]) :
    fd(-1), id(-1), serverPort(-1), serverIp(""), connected(false), debugMode(false)
{
    // Set up signal handler for graceful shutdown
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // Parse command line arguments
    parseArguments(ac, av);
    
    if (debugMode) {
        std::cout << "[CLIENT] Debug mode enabled" << std::endl;
        std::cout << "[CLIENT] Server IP: " << serverIp << std::endl;
        std::cout << "[CLIENT] Server Port: " << serverPort << std::endl;
    }
}

// Destructor
ClientModule::Client::~Client() {
    if (connected) {
        close(fd);
    }
    
    // Make sure threads are stopped
    stop();
    
    if (debugMode) {
        std::cout << "[CLIENT] Destroyed client" << std::endl;
    }
}

// Main client run function
void ClientModule::Client::run() {
    // Create socket
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throw std::runtime_error("Failed to create socket");
    }
    
    // Set up server address
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    if (inet_pton(AF_INET, serverIp.c_str(), &serverAddr.sin_addr) <= 0) {
        throw std::runtime_error("Invalid server address");
    }
    
    // Connect to server
    if (connect(fd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        throw std::runtime_error("Connection failed");
    }
    
    connected = true;
    address = serverAddr;
    
    if (debugMode) {
        std::cout << "[CLIENT] Connected to server at " << getAddress() << std::endl;
    }
    
    // Start threads
    startThread();
    
    // Wait for threads to finish
    runThread();
}

// Stop client
void ClientModule::Client::stop() {
    if (connected) {
        connected = false;
        
        // Wait a short time for threads to terminate cleanly
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Close socket
        close(fd);
        fd = -1;
        
        if (debugMode) {
            std::cout << "[CLIENT] Disconnected from server" << std::endl;
        }
    }
}

// Get formatted server address
std::string ClientModule::Client::getAddress() const {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &address.sin_addr, ip, INET_ADDRSTRLEN);
    return std::string(ip) + ":" + std::to_string(ntohs(address.sin_port));
}

// Network thread function - handles communication with the server
void ClientModule::Client::networkThread() {
    if (debugMode) {
        std::cout << "[CLIENT] Network thread started" << std::endl;
    }
    
    // Buffer for receiving data
    PacketModule incomingPacket;
    PacketModule outgoingPacket;
    
    // Set socket to non-blocking mode
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    
    // Main network loop
    while (connected && !g_shutdown) {
        // Receive data from server
        int result = recv(fd, &incomingPacket, sizeof(PacketModule::Packet), MSG_DONTWAIT);
        
        if (result > 0) {
            // Process received data
            if (debugMode) {
                std::lock_guard<std::mutex> lock(_packetMutex);
                std::cout << "[CLIENT] Received packet from server" << std::endl;
                incomingPacket.display("[CLIENT] Received: ");
            }
            
            // Update shared game state
            {
                std::lock_guard<std::mutex> lock(_packetMutex);
                
                // First time receiving a packet, set client ID
                if (id == -1) {
                    id = incomingPacket.getClientId();
                    if (debugMode) {
                        std::cout << "[CLIENT] Assigned client ID: " << id << std::endl;
                    }
                }
                
                // Update packet with our client ID
                packet = incomingPacket;
            }
        } else if (result == -1) {
            // Non-blocking socket returned EAGAIN or EWOULDBLOCK
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                if (debugMode) {
                    std::cerr << "[CLIENT] Network error: " << strerror(errno) << std::endl;
                }
                connected = false;
                break;
            }
        } else if (result == 0) {
            // Server closed connection
            if (debugMode) {
                std::cout << "[CLIENT] Server closed connection" << std::endl;
            }
            connected = false;
            break;
        }
        
        // Send data to server
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
        }
        
        if (debugMode) {
            std::lock_guard<std::mutex> lock(_packetMutex);
            outgoingPacket.display("[CLIENT] Sent: ");
        }
        
        // Short sleep to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    if (debugMode) {
        std::cout << "[CLIENT] Network thread stopped" << std::endl;
    }
}

// Start threads
void ClientModule::Client::startThread() {
    if (debugMode) {
        std::cout << "[CLIENT] Starting threads" << std::endl;
    }
    
    try {
        _networkThread = std::thread(&Client::networkThread, this);
        _gameThread = std::thread(&Client::gameThread, this);
    } catch (const std::system_error& e) {
        std::cerr << "[CLIENT] Thread startup failed: " << e.what() << std::endl;
        connected = false;
        throw;
    }
}

// Wait for threads to finish
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