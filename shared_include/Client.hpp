#pragma once
#include "Packet.hpp"
#include "Map.hpp"
#include "Game.hpp"
#include <mutex>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <vector>
#include <condition_variable>
#include "Error.hpp"

namespace ClientModule {
    class Client {
        public:
            struct ReceivedFileInfo {
                std::string name;
                uintmax_t size;
            };
            Client(int ac, char* av[]);
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
            
    // Map Handling
            void receiveMapData();

    // File Transfer Handling
            void receiveAssets(int serverSocket);
            void receiveFile(int serverSocket, const std::string& savePath);
            std::vector<ReceivedFileInfo> receiveDirectoryData(int serverSocket);
            
       private:
            void parseArguments(int argc, const char *argv[]);
            
            // Threads
            std::thread _gameThread;
            std::thread _networkThread;
            
            // Network
            int fd;
            int id;
            int serverPort;
            std::string serverIp;
            sockaddr_in address;
            bool connected;
            bool debugMode;
            
            // Game state
            PacketModule _currentPacket;
            std::mutex _packetMutex;
            
            // Map
            Map _map;
            bool _mapLoaded;
            std::mutex _mapMutex;
            std::condition_variable _mapCV;
    };
};