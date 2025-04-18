#pragma once 
#include <vector>
#include <utility>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>
#include "Error.hpp"

class PacketModule {
    public:
        PacketModule();
        PacketModule(int nb_clients);
        PacketModule(const PacketModule& other);
        ~PacketModule();

        enum gameState {
            PLAYING = 0,
            WAITING,
            ENDED,
        };

        struct Packet {
            Packet& operator=(const Packet& other) {
                if (this != &other) {
                    nb_client = other.nb_client;
                    client_id = other.client_id;
                    for (int i = 0; i < MAX_CLIENTS; ++i) {
                        playerState[i] = other.playerState[i];
                        playerPosition[i] = other.playerPosition[i];
                    }
                }
                return *this;
            }
            int nb_client;
            int client_id;
            gameState playerState[MAX_CLIENTS];
            std::pair<int, int> playerPosition[MAX_CLIENTS];
        };
        void display(std::string info);
        void setPacket(const struct Packet& pkt);
        Packet& getPacket() { return pkt; }
        const Packet& getPacket() const { return pkt; }  // Ajout d'une version const
        int getNbClient() const;
        int getClientId() const;
        gameState getstate() const;
        std::pair<int, int> getPosition() const;
    private:
        struct Packet pkt;
};