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

void ClientModule::Client::gameThread()
{
    // Initialiser le moteur graphique
    if (!_graphics.init(800, 600, "Jetpack - Client " + std::to_string(id))) {
        std::cerr << "Failed to initialize graphics engine" << std::endl;
        connected = false;
        return;
    }
    
    // Charger les ressources graphiques
    if (!_graphics.loadResources()) {
        std::cerr << "Failed to load graphical resources" << std::endl;
        connected = false;
        return;
    }
    
    // Recevoir la carte du serveur (à implémenter)
    receiveMap();
    
    // Définir la carte dans le moteur graphique
    _graphics.setMap(_map);
    
    // Boucle principale du jeu
    while (connected && _graphics.isOpen()) {
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
    }
    
    // Si on sort de la boucle et que la fenêtre est encore ouverte, on la ferme
    if (_graphics.isOpen()) {
        // La fenêtre se fermera automatiquement quand l'objet _graphics sera détruit
    }
    
    // Signaler la déconnexion
    connected = false;
}

void ClientModule::Client::receiveMap()
{
    // Pour l'instant, on crée une carte par défaut
    // Cette méthode devrait normalement recevoir les données de la carte du serveur
    
    const int width = 20;
    const int height = 10;
    
    _map.resize(height, std::vector<TileType>(width, EMPTY));
    
    // Ajouter des murs aux bords
    for (int x = 0; x < width; ++x) {
        _map[0][x] = WALL;
        _map[height - 1][x] = WALL;
    }
    
    for (int y = 0; y < height; ++y) {
        _map[y][0] = WALL;
        _map[y][width - 1] = WALL;
    }
    
    // Ajouter quelques plateformes
    for (int x = 5; x < 15; ++x) {
        _map[3][x] = WALL;
        _map[6][x] = WALL;
    }
    
    // Ajouter des pièces
    _map[1][5] = COIN;
    _map[1][10] = COIN;
    _map[1][15] = COIN;
    _map[4][7] = COIN;
    _map[4][12] = COIN;
    _map[7][8] = COIN;
    _map[7][14] = COIN;
    
    // Ajouter des zones électriques
    _map[2][8] = ELECTRIC;
    _map[5][10] = ELECTRIC;
    _map[8][5] = ELECTRIC;
    
    // Ajouter les points de départ
    _map[1][2] = START_P1;
    _map[7][2] = START_P2;
    
    // Ajouter la ligne d'arrivée
    _map[4][width - 2] = FINISH;
    _map[8][width - 2] = FINISH;
    
    if (debugMode) {
        std::cout << "[CLIENT] Created default map" << std::endl;
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
        } else {
            if (debugMode) {
                std::lock_guard<std::mutex> lock(_packetMutex);
                newGameState.display("[CLIENT]: Recv: ");
            }
            {
                std::lock_guard<std::mutex> lock(_packetMutex);
                packet.setPacket(newGameState.getPacket());
            }
        }
        
        PacketModule outgoingPacket;
        {
            std::lock_guard<std::mutex> lock(_packetMutex);
            outgoingPacket.setPacket(packet.getPacket());
            outgoingPacket.setPacketType(PacketModule::PLAYER_INPUT);
        }
        
        if (send(fd, &outgoingPacket.getPacket(), sizeof(PacketModule::Packet), MSG_DONTWAIT) == -1) {
            connected = false;
        }
        
        if (debugMode) {
            std::lock_guard<std::mutex> lock(_packetMutex);
            outgoingPacket.display("[CLIENT]: Send: ");
        }
        
        // Attendre un court instant pour éviter une utilisation excessive du CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
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
