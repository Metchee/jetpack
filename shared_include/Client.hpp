#pragma once
#include "Packet.hpp"
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>

namespace ClientModule {
    struct Game {
      int nb_player;
      Player player;
      Player otherPlayer[nb_player - 1];
      int score;
      char map[2000];
    };
    struct Player {
        std::pair<int, int> position;
        int score;
        PacketModule::gameState state;
    };
    class Client {
        public:
            Client(int ac, const char *av[]);
            ~Client();

            void run();
            void stop();

            std::string getAddress() const;
            int getFd() const { return fd; };
            int getId() const { return id; }
            void setId(int id) { this->id = id; }
            bool isConnected() const { return connected; }
            void gameThread();
            void networkThread();
        private:
            std::vector<char> receive(size_t maxSize);
            void send(const struct Packet& data);
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