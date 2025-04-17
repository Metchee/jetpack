#pragma once
#include "Packet.hpp"
#include <mutex>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <vector>
#include "Error.hpp"


namespace ClientModule {
    class Client {
        public:
            struct ReceivedFileInfo {
                std::string name;
                uintmax_t size;
            };
            Client(int ac, const char *av[]);
            ~Client();

    // Main Running Function
            void run();
            void stop();
            
    // Network Handling
            std::string getAddress() const;
            int getFd() const { return fd; };
            int getId() const { return id; }
            void setId(int id) { this->id = id; }
            bool isConnected() const { return connected; }

    // Thread Handling
            void gameThread();
            void networkThread();
            void startThread();
            void runThread();
            std::mutex _packetMutex;

    // File Transfer Handling
           
            void receiveAssets(int serverSocket);
            void receiveFile(int serverSocket, const std::string& savePath);
            std::vector<ReceivedFileInfo> receiveDirectoryData(int serverSocket);
       private:
            std::thread _gameThread;
            std::thread _networkThread;
            void parseArguments(int argc, const char *argv[]);
            PacketModule packet;
            int fd;
            int id;
            int serverPort;
            std::string serverIp;
            sockaddr_in address;
            bool connected;
            bool debugMode;
    };
};
