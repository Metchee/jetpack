#pragma once
#include <vector>
#include <memory>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include <unordered_map>
#include <chrono>
#include "Config.hpp"
#include "Packet.hpp"
#include "Game.hpp"

class Server {
public:
    struct FileInfo {
        std::string name;
        std::string path;
        uintmax_t size;
    };
    
    Server(int argc, const char* argv[]);
    ~Server();
    
    void run();
    void stop();
    
private:
    // File transfer
    std::vector<FileInfo> getDirectoryList(const std::string &directoryPath);
    void sendDirectoryList(int clientSocket, const std::string &directoryPath);
    void sendFile(int clientSocket, const std::string& filePath);
    
    // Server management
    void handleNewConnection();
    void handleClientData(int client_fd);
    void removeClient(int fd);
    
    // Game logic
    void updateGame();
    void sendMapData(int client_fd);
    
    // Packets handling
    void broadcastGameState();
    int sendPacket(int client_fd, PacketModule &packetModule);
    bool readPacket(int client_fd, PacketModule &packetModule);

    // Member variables
    bool _packetsUpdated;
    int _serverFd;
    int _nbClients;
    
    std::vector<std::shared_ptr<pollfd>> _fdsList;
    std::unordered_map<int, PacketModule> _packets;
    std::unordered_map<int, int> _clientIds; 
    
    std::unique_ptr<Game> _game;
    std::chrono::time_point<std::chrono::steady_clock> _lastUpdateTime;
    
    ServerConfig config;
};