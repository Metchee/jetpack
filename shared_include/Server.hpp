#pragma once
#include <vector>
#include <memory>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include "Config.hpp"
#include "Packet.hpp"
#include <mutex>
#include <unordered_map>

class Server {
    public:
        Server(int argc, char* argv[]);
        ~Server();
        void run();
        void stop();
    private:
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
        bool _running;
        std::mutex _clientsMutex;
        std::vector<std::shared_ptr<pollfd>> _fdsList;
        std::unordered_map<int, PacketModule> _packets;
        std::unordered_map<int, int> _clientIds; 
        ServerConfig config;
};
