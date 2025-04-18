#include "../shared_include/Client.hpp"
#include "../shared_include/ClientGraphics.hpp"
#include "../shared_include/Map.hpp"
#include <cstdio>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <fstream>

void ClientModule::Client::receiveMapData()
{
    // Receive map size
    uint32_t mapSize;
    ssize_t bytes_read = recv(fd, &mapSize, sizeof(mapSize), 0);
    
    if (bytes_read <= 0 || bytes_read != sizeof(mapSize)) {
        throw std::runtime_error("Failed to receive map size from server");
    }
    
    if (debugMode) {
        std::cout << "Receiving map data (size: " << mapSize << " bytes)" << std::endl;
    }
    
    // Receive map data
    std::vector<char> mapData(mapSize);
    size_t totalRead = 0;
    
    while (totalRead < mapSize) {
        bytes_read = recv(fd, mapData.data() + totalRead, mapSize - totalRead, 0);
        
        if (bytes_read <= 0) {
            throw std::runtime_error("Failed to receive map data from server");
        }
        
        totalRead += bytes_read;
    }
    
    // Parse map data
    std::string mapString(mapData.begin(), mapData.end());
    
    {
        std::lock_guard<std::mutex> lock(_mapMutex);
        if (!_map.deserialize(mapString)) {
            throw std::runtime_error("Failed to parse map data");
        }
        
        _mapLoaded = true;
        _mapCV.notify_all();
    }
    
    if (debugMode) {
        std::cout << "Map loaded successfully (" << _map.getWidth() << "x" << _map.getHeight() << ")" << std::endl;
    }
}

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
    fd(-1), id(-1), serverPort(-1), serverIp(""), connected(false),
    debugMode(false), _mapLoaded(false)
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
    // Connect to server
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
    
    // Receive initial packet to get client ID
    PacketModule initialPacket;
    ssize_t bytes_read = recv(fd, &initialPacket.getPacket(), sizeof(PacketModule::Packet), 0);
    
    if (bytes_read <= 0) {
        throw std::runtime_error("Failed to receive initial packet from server");
    }
    
    id = initialPacket.getClientId();
    
    if (debugMode) {
        std::cout << "Assigned client ID: " << id << std::endl;
    }
    
    // Receive map data
    receiveMapData();
    
    // Start game threads
    startThread();
    
    // Wait for threads to finish
    if (_gameThread.joinable()) {
        _gameThread.join();
    }
    
    if (_networkThread.joinable()) {
        _networkThread.join();
    }
}

void ClientModule::Client::stop() {
    connected = false;
    
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
}

std::string ClientModule::Client::getAddress() const {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &address.sin_addr, ip, INET_ADDRSTRLEN);
    return std::string(ip) + ":" + std::to_string(ntohs(address.sin_port));
}

void ClientModule::Client::gameThread()
{
    // Create graphics window
    ClientGraphics graphics(800, 600);
    
    // Wait for map to be loaded
    {
        std::unique_lock<std::mutex> lock(_mapMutex);
        _mapCV.wait(lock, [this]{ return _mapLoaded; });
        
        // Load map into graphics
        graphics.loadMap(_map);
    }
    
    GameState gameState;
    gameState.nbPlayers = 0;
    gameState.clientId = id;
    gameState.gameStarted = false;
    gameState.gameOver = false;
    
    while (connected && graphics.isWindowOpen()) {
        // Process input
        Direction inputDir = graphics.processEvents();
        
        // Update input packet
        {
            std::lock_guard<std::mutex> lock(_packetMutex);
            
            // Set input direction in packet (specific implementation depends on protocol)
            // For now, we're just setting a position change based on direction
            auto& pkt = _currentPacket.getPacket();
            std::pair<int, int> pos = pkt.playerPosition[id];
            
            switch (inputDir) {
                case Direction::UP:
                    pos.second -= 1;
                    break;
                case Direction::DOWN:
                    pos.second += 1;
                    break;
                case Direction::LEFT:
                    pos.first -= 1;
                    break;
                case Direction::RIGHT:
                    pos.first += 1;
                    break;
                default:
                    break;
            }
            
            pkt.playerPosition[id] = pos;
        }
        
        // Update game state for rendering
        {
            std::lock_guard<std::mutex> lock(_packetMutex);
            const auto& pkt = _currentPacket.getPacket();
            
            gameState.nbPlayers = pkt.nb_client;
            gameState.playerPositions.resize(pkt.nb_client);
            gameState.playerState.resize(pkt.nb_client);
            gameState.playerScores.resize(pkt.nb_client, 0); // Scores not implemented yet
            
            for (int i = 0; i < pkt.nb_client; i++) {
                gameState.playerPositions[i] = pkt.playerPosition[i];
                gameState.playerState[i] = pkt.playerState[i];
                
                // Determine game state
                if (pkt.playerState[i] == PacketModule::PLAYING) {
                    gameState.gameStarted = true;
                }
                
                if (pkt.playerState[i] == PacketModule::ENDED) {
                    gameState.gameOver = true;
                }
            }
            
            // Center view on our player
            if (id >= 0 && id < gameState.nbPlayers) {
                graphics.centerViewOnPlayer(gameState.playerPositions[id]);
            }
        }
        
        // Render game
        graphics.render(gameState);
        
        // Limit frame rate
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60fps
    }
    
    connected = false;
}

void ClientModule::Client::networkThread()
{
    while (connected) {
        // Prepare packet to send
        PacketModule outgoingPacket;
        {
            std::lock_guard<std::mutex> lock(_packetMutex);
            outgoingPacket = _currentPacket;
        }
        
        // Send packet to server
        if (send(fd, &outgoingPacket.getPacket(), sizeof(PacketModule::Packet), 0) <= 0) {
            if (debugMode) {
                std::cerr << "Failed to send packet to server" << std::endl;
            }
            connected = false;
            break;
        }
        
        // Receive packet from server
        PacketModule incomingPacket;
        ssize_t bytes_read = recv(fd, &incomingPacket.getPacket(), sizeof(PacketModule::Packet), 0);
        
        if (bytes_read <= 0) {
            if (debugMode) {
                std::cerr << "Failed to receive packet from server" << std::endl;
            }
            connected = false;
            break;
        }
        
        // Update current packet with server data
        {
            std::lock_guard<std::mutex> lock(_packetMutex);
            _currentPacket = incomingPacket;
            
            if (debugMode) {
                _currentPacket.display("[CLIENT] Received: ");
            }
        }
        
        // Small delay to avoid spinning
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

void ClientModule::Client::startThread()
{
    _gameThread = std::thread(&Client::gameThread, this);
    _networkThread = std::thread(&Client::networkThread, this);
}