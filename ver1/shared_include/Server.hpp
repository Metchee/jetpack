#pragma once
#include <vector>
#include <memory>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include "Config.hpp"
#include "Packet.hpp"
#include <unordered_map>
#include "Game.hpp"
#include <chrono>

class Server {
    public:
        struct FileInfo {
            std::string name;
            std::string path;
            uintmax_t size;
        };
        Server(int argc, char* argv[]);
        ~Server();
        void run();
        void stop();
    private:
    // file transfer
        std::vector<FileInfo> getDirectoryList(const std::string &directoryPath);
        void sendDirectoryList(int clientSocket, const std::string &directoryPath);
        void sendFile(int clientSocket, const std::string& filePath);
    // server management
        void handleNewConnection();
        void handleClientData(int client_fd);
        void removeClient(int fd);
    // packets handling
        void broadcastPackets();
        int sendPacket(int client_fd, PacketModule &packetModule);
        bool readPacket(int client_fd, PacketModule &packetModule);

    // local variables
        bool _packetsUpdated;
        int _serverFd;
        int _nbClients;
        std::vector<std::shared_ptr<pollfd>> _fdsList;
        std::unordered_map<int, PacketModule> _packets;
        std::unordered_map<int, int> _clientIds; 
        ServerConfig config;
        Game _game;
        
        // État du jeu
        bool _gameStarted;
        bool _gameWaitingForPlayers;
        
        // Horodatage pour le calcul du delta time
        std::chrono::time_point<std::chrono::high_resolution_clock> _lastUpdateTime;
        
        // Méthodes de gestion du jeu
        void startGame();
        void updateGame();
        void sendGameState();
        void handlePlayerInput(int clientId, const PacketModule& packet);
        
        // Envoyer la carte aux clients
        void sendMapToClient(int clientFd);
};
