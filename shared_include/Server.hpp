#pragma once
#include <vector>
#include <memory>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include "Config.hpp"
#include "Packet.hpp"
#include <unordered_map>
class Server {
    public:
        Server(int argc, char* argv[]);
        ~Server();
        void run();
        void stop();
    private:
        void loadMap();
        void handleNewConnection();
        void handleClientData(int client_fd);
        void removeClient(int fd);
        void sendPacket(int client_fd, PacketModule& packet);
        bool readPacket(int client_fd, PacketModule& packet);

        int _serverFd;
        int _nbClients;
        std::vector<std::shared_ptr<pollfd>> _fdsList;
        std::unordered_map<int, PacketModule> _packets;
        std::unordered_map<int, int> _clientIds; 
        ServerConfig config;
};
