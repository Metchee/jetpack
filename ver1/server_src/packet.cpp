#include "Packet.hpp"
#include <iostream>

PacketModule::PacketModule() : pkt{}
{
    pkt.nb_client = 0;
    pkt.client_id = 0;
    pkt.type = GAME_STATE;
    std::fill_n(pkt.playerState, MAX_CLIENTS, WAITING);
    std::fill_n(pkt.playerPosition, MAX_CLIENTS, std::make_pair(0, 0));
    std::fill_n(pkt.playerScore, MAX_CLIENTS, 0);
    std::fill_n(pkt.jetpackActive, MAX_CLIENTS, false);
}

PacketModule::PacketModule(int nb_clients) : pkt{}
{
    pkt.nb_client = nb_clients;
    pkt.client_id = 0;
    pkt.type = GAME_STATE;
    std::fill_n(pkt.playerState, MAX_CLIENTS, WAITING);
    std::fill_n(pkt.playerPosition, MAX_CLIENTS, std::make_pair(0, 0));
    std::fill_n(pkt.playerScore, MAX_CLIENTS, 0);
    std::fill_n(pkt.jetpackActive, MAX_CLIENTS, false);
}

PacketModule::PacketModule(const PacketModule& other) : pkt(other.pkt) {}
PacketModule::~PacketModule() {}

PacketModule::gameState PacketModule::getstate() const
{ 
    return pkt.playerState[pkt.client_id]; 
}

std::pair<int, int> PacketModule::getPosition() const
{ 
    return pkt.playerPosition[pkt.client_id]; 
}

int PacketModule::getClientId() const
{ 
    return pkt.client_id; 
}

int PacketModule::getNbClient() const
{ 
    return pkt.nb_client; 
}

int PacketModule::getScore() const
{
    return pkt.playerScore[pkt.client_id];
}

bool PacketModule::getJetpackActive() const
{
    return pkt.jetpackActive[pkt.client_id];
}

void PacketModule::setJetpackActive(bool active)
{
    pkt.jetpackActive[pkt.client_id] = active;
}

PacketModule::packetType PacketModule::getPacketType() const
{
    return pkt.type;
}

void PacketModule::setPacketType(packetType type)
{
    pkt.type = type;
}

void PacketModule::setPacket(const struct Packet& pkt)
{
    this->pkt = pkt;
}

void PacketModule::display(std::string info)
{
    std::cout << info << "Packet type: " << pkt.type << ", Client ID: " << pkt.client_id << std::endl;
    std::cout << info << "Number of clients: " << pkt.nb_client << std::endl;
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (i < pkt.nb_client) {
            std::cout << info << "Player " << i << " state: " << pkt.playerState[i]
                      << ", Position: (" << pkt.playerPosition[i].first << ", " << pkt.playerPosition[i].second << ")"
                      << ", Score: " << pkt.playerScore[i]
                      << ", Jetpack: " << (pkt.jetpackActive[i] ? "ON" : "OFF") << std::endl;
        }
    }
}