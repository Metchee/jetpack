#pragma once 
#include <vector>
#include <utility>
#include "Error.hpp"

class PacketModule {
    public:
        PacketModule(int nb_clients);
        ~PacketModule() = default;
        enum gameState {
            PLAYING = 0,
            WAITING,
            ENDED,
        };
        struct Packet {
            int nb_client;
            int client_id;
            gameState playerState[MAX_CLIENT];
            std::pair<int, int> playerPosition[MAX_CLIENT];
            char map[2000];
        };
        int getNbClient() const;
        int getClientId() const;
        gameState getstate() const;
        std::pair<int, int> getPosition() const;
        const char *getMap() const;
        struct Packet pkt;
    private:
        int nb_client;
        int client_id;
        std::vector <gameState> playerState;
        std::vector <std::pair<int, int>> playerPosition;
        char map[1024];
};
