#include "../shared_include/Client.hpp"
#include <cstdio>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <iostream>

void ClientModule::Client::parseArguments(int argc, const char *argv[])
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

ClientModule::Client::Client(int ac, const char *av[]) :
    fd(-1), id(-1), serverPort(-1), serverIp(""), connected(false)
{
    parseArguments(ac, av);
}

ClientModule::Client::~Client() {
    if (connected) {
        close(fd);
    }
}
void ClientModule::Client::run()
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
    startThread();
    runThread();
}

void ClientModule::Client::stop() {
    connected = false;
}

std::string ClientModule::Client::getAddress() const {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &address.sin_addr, ip, INET_ADDRSTRLEN);
    return std::string(ip) + ":" + std::to_string(ntohs(address.sin_port));
}

void ClientModule::Client::gameThread(/* Game structure*/)
{
    while(connected) {
        // get player_input = getPlayerInput();
        PacketModule current_state;
        {
            std::lock_guard<std::mutex> lock(_packetMutex);
            current_state.setPacket(packet.getPacket());
        }
        // if (debugMode) {
        //     std::lock_guard<std::mutex> lock(_packetMutex);
        //     current_state.display("[CLIENT] Game");
        // }
        // renderGame(current_state);
    }
    
}
void ClientModule::Client::networkThread()
{
    PacketModule newGameState;

    while (connected) {
        if (recv(fd, &newGameState, sizeof(PacketModule::Packet), MSG_DONTWAIT) == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                connected = false;
            }
        }
        if (debugMode) {
            std::lock_guard<std::mutex> lock(_packetMutex);
            newGameState.display("[CLIENT]: Recv: ");
        }
        {
            std::lock_guard<std::mutex> lock(_packetMutex);
            packet.setPacket(newGameState.getPacket());
        }
        PacketModule outgoingPacket;
        {
            std::lock_guard<std::mutex> lock(_packetMutex);
            outgoingPacket.setPacket(packet.getPacket());
        }
        if (send(fd, &outgoingPacket.getPacket(), sizeof(PacketModule::Packet), MSG_DONTWAIT) == -1) {
            connected = false;
        }
        if (debugMode) {
            std::lock_guard<std::mutex> lock(_packetMutex);
            outgoingPacket.display("[CLIENT]: Send: ");
        }
    }
}


void ClientModule::Client::startThread()
{
    try {
        _gameThread = std::thread(&Client::gameThread, this);
        _networkThread = std::thread(&Client::networkThread, this);
    } catch (const std::system_error& e) {
        std::cerr << "Thread startup failed: " << e.what() << "\n";
        connected = false;
    }
}

void ClientModule::Client::runThread()
{
    if (_gameThread.joinable()) {
        _gameThread.join();
    }
    if (_networkThread.joinable()) {
        _networkThread.join();
    }

    _gameThread = std::thread([this]() {
        gameThread(); 
    });
    _networkThread = std::thread([this]() {
        networkThread();
    }); 
}
