#pragma once
#include "Packet.hpp"
#include <mutex>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <vector>
#include <SFML/Graphics.hpp> // Add SFML graphics
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

        // Graphics-related members
        sf::RenderWindow window; // SFML window
        sf::Font font; // For rendering text (e.g., scores, debug info)
        sf::Texture playerTexture, coinTexture, zapperTexture; // Textures for sprites
        sf::Sprite playerSprite, coinSprite, zapperSprite; // Sprites for rendering
        std::vector<std::vector<char>> map; // Parsed map for rendering
        int mapWidth, mapHeight; // Map dimensions
    };
};