#include "../shared_include/Client.hpp"
#include <cstdio>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <iostream>
#include <fstream>

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
    fd(-1), id(-1), serverPort(-1), serverIp(""), connected(false), debugMode(false)
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

void ClientModule::Client::startThread()
{
    if (debugMode) {
        std::cout << "[CLIENT] Starting game and network threads" << std::endl;
    }
    
    try {
        // Créer les threads initiaux
        _gameThread = std::thread(&Client::gameThread, this);
        _networkThread = std::thread(&Client::networkThread, this);
    } catch (const std::system_error& e) {
        std::cerr << "Thread startup failed: " << e.what() << "\n";
        connected = false;
    }
}

void ClientModule::Client::runThread()
{
    // Attendre que les threads se terminent
    if (_gameThread.joinable()) {
        _gameThread.join();
    }
    if (_networkThread.joinable()) {
        _networkThread.join();
    }
    
    // Vous pourriez ajouter une logique ici pour redémarrer les threads si nécessaire
    // Mais normalement, une fois que les threads se terminent, le client devrait aussi terminer
}
void ClientModule::Client::stop() {
    connected = false;
}

std::string ClientModule::Client::getAddress() const {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &address.sin_addr, ip, INET_ADDRSTRLEN);
    return std::string(ip) + ":" + std::to_string(ntohs(address.sin_port));
}

void ClientModule::Client::gameThread()
{
    // Initialiser le moteur graphique
    if (!_graphics.init(1024, 768, "Jetpack Joyride - Client")) {
        std::cerr << "Failed to initialize graphics engine" << std::endl;
        connected = false;
        return;
    }
    
    if (debugMode) {
        std::cout << "[CLIENT] Graphics engine initialized successfully" << std::endl;
    }
    
    // Charger les ressources graphiques
    if (!_graphics.loadResources()) {
        std::cerr << "Failed to load graphical resources" << std::endl;
        connected = false;
        return;
    }
    
    if (debugMode) {
        std::cout << "[CLIENT] Graphics resources loaded successfully" << std::endl;
    }
    
    // Recevoir la carte du serveur
    receiveMap();
    
    // Définir la carte dans le moteur graphique
    _graphics.setMap(_map);
    
    // Boucle principale du jeu
    sf::Clock gameClock;
    while (connected && _graphics.isOpen()) {
        // Limiter le taux de rafraîchissement
        sf::Time frameTime = gameClock.restart();
        
        // Obtenir l'état du jeu actuel
        {
            std::lock_guard<std::mutex> lock(_packetMutex);
            _graphics.updateGameState(packet, _packetMutex);
        }
        
        // Mettre à jour l'entrée du joueur dans le paquet
        {
            std::lock_guard<std::mutex> lock(_packetMutex);
            packet.setJetpackActive(_graphics.getJetpackActive());
        }
        
        // Faire tourner le moteur graphique (une itération)
        _graphics.run();
        
        // Attendre un peu pour éviter d'utiliser trop de CPU
        sf::sleep(sf::milliseconds(16)); // ~60 FPS
    }
    
    if (debugMode) {
        std::cout << "[CLIENT] Game loop exited" << std::endl;
    }
    
    // Signaler la déconnexion
    connected = false;
}

void ClientModule::Client::receiveMap()
{
    // Try to receive map from server
    bool mapReceived = false;
    
    // Wait for a short time to see if we receive map data
    if (debugMode) {
        std::cout << "[CLIENT] Waiting for map data from server..." << std::endl;
    }
    
    // We'll use a timeout to avoid waiting forever
    int attempts = 0;
    const int MAX_ATTEMPTS = 10;
    
    while (!mapReceived && attempts < MAX_ATTEMPTS) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Check if we've received a map packet
        {
            std::lock_guard<std::mutex> lock(_packetMutex);
            if (packet.getPacketType() == PacketModule::MAP_DATA) {
                // TODO: Process actual map data when server sends it
                mapReceived = true;
                if (debugMode) {
                    std::cout << "[CLIENT] Received map data from server" << std::endl;
                }
            }
        }
        
        attempts++;
    }
    
    if (!mapReceived) {
        if (debugMode) {
            std::cout << "[CLIENT] No map data received, creating default map" << std::endl;
        }
    }
    
    // For now, we'll still create a default map to ensure we have something to display
    const int width = 100;  // Much wider map for scrolling
    const int height = 20;
    
    _map.resize(height, std::vector<TileType>(width, EMPTY));
    
    // Add a ground level
    for (int x = 0; x < width; ++x) {
        _map[height - 1][x] = WALL;
    }
    
    // Add ceiling
    for (int x = 0; x < width; ++x) {
        _map[0][x] = WALL;
    }
    
    // Add some platforms at varying heights
    for (int x = 10; x < 20; ++x) {
        _map[height - 5][x] = WALL;
    }
    
    for (int x = 25; x < 35; ++x) {
        _map[height - 7][x] = WALL;
    }
    
    for (int x = 40; x < 55; ++x) {
        _map[height - 9][x] = WALL;
    }
    
    for (int x = 60; x < 70; ++x) {
        _map[height - 6][x] = WALL;
    }
    
    for (int x = 75; x < 90; ++x) {
        _map[height - 8][x] = WALL;
    }
    
    // Add coins in patterns
    for (int x = 5; x < width - 5; x += 5) {
        _map[height - 4][x] = COIN;
    }
    
    // Create coin arcs
    for (int i = 0; i < 3; ++i) {
        int centerX = 20 + i * 30;
        int centerY = height - 8;
        int radius = 5;
        
        for (int j = 0; j < 7; ++j) {
            int x = centerX + j;
            int y = centerY - static_cast<int>(radius * sin(j * 3.14159 / 6));
            if (y > 0 && y < height && x > 0 && x < width) {
                _map[y][x] = COIN;
            }
        }
    }
    
    // Add electric hazards
    _map[height - 3][15] = ELECTRIC;
    _map[height - 3][30] = ELECTRIC;
    _map[height - 3][45] = ELECTRIC;
    _map[height - 3][60] = ELECTRIC;
    _map[height - 3][75] = ELECTRIC;
    
    // Add some suspended electric hazards
    _map[height - 10][25] = ELECTRIC;
    _map[height - 12][50] = ELECTRIC;
    _map[height - 8][75] = ELECTRIC;
    
    // Add starting positions
    _map[height - 3][2] = START_P1;
    _map[height - 3][4] = START_P2;
    
    // Add finish line
    _map[5][width - 2] = FINISH;
    _map[6][width - 2] = FINISH;
    _map[7][width - 2] = FINISH;
    _map[8][width - 2] = FINISH;
    
    if (debugMode) {
        std::cout << "[CLIENT] Created default map with dimensions: " << width << "x" << height << std::endl;
    }
}


void ClientModule::Client::networkThread()
{
    PacketModule newGameState;

    while (connected) {
        // Try to receive game state from server
        ssize_t recvSize = recv(fd, &newGameState, sizeof(PacketModule::Packet), MSG_DONTWAIT);
        
        if (recvSize == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                if (debugMode) {
                    std::cerr << "[CLIENT] Error receiving data: " << strerror(errno) << std::endl;
                }
                connected = false;
            }
        } else if (recvSize == 0) {
            // Connection closed by server
            if (debugMode) {
                std::cout << "[CLIENT] Server closed the connection" << std::endl;
            }
            connected = false;
        } else if (recvSize == sizeof(PacketModule::Packet)) {
            // Successfully received a full packet
            if (debugMode) {
                std::lock_guard<std::mutex> lock(_packetMutex);
                newGameState.display("[CLIENT] Received: ");
            }
            
            // Update our local packet with the received data
            {
                std::lock_guard<std::mutex> lock(_packetMutex);
                packet.setPacket(newGameState.getPacket());
            }
        }
        
        // Send player input to server (mainly jetpack state)
        PacketModule outgoingPacket;
        {
            std::lock_guard<std::mutex> lock(_packetMutex);
            outgoingPacket.setPacket(packet.getPacket());
            outgoingPacket.setPacketType(PacketModule::PLAYER_INPUT);
        }
        
        ssize_t sendSize = send(fd, &outgoingPacket.getPacket(), sizeof(PacketModule::Packet), MSG_DONTWAIT);
        
        if (sendSize == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
            if (debugMode) {
                std::cerr << "[CLIENT] Error sending data: " << strerror(errno) << std::endl;
            }
            connected = false;
        } else if (sendSize == sizeof(PacketModule::Packet) && debugMode) {
            std::lock_guard<std::mutex> lock(_packetMutex);
            outgoingPacket.display("[CLIENT] Sent: ");
        }
        
        // Limit update rate to reduce CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60Hz
    }
    
    if (debugMode) {
        std::cout << "[CLIENT] Network thread exited" << std::endl;
    }
}