#include "Packet.hpp"
#include <iostream>


PacketModule::PacketModule() : pkt{}
{
    pkt.nb_client = 0;
    pkt.client_id = 0;
    std::fill_n(pkt.playerState, MAX_CLIENTS, WAITING);
    std::fill_n(pkt.playerPosition, MAX_CLIENTS, std::make_pair(0, 0));
}

PacketModule::PacketModule(int nb_clients) : pkt{}
{
    pkt.nb_client = nb_clients;
    pkt.client_id = 0;
    std::fill_n(pkt.playerState, MAX_CLIENTS, WAITING);
    std::fill_n(pkt.playerPosition, MAX_CLIENTS, std::make_pair(0, 0));
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

void PacketModule::setPacket(const struct Packet& pkt)
{
    this->pkt = pkt;
}

void PacketModule::display(std::string info)
{
    
        std::cout << info << " ID: " << pkt.client_id << std::endl;
        std::cout << info << " Number of clients: " << pkt.nb_client << std::endl;
        std::cout << info << " Player state: " << pkt.playerState[pkt.client_id] << std::endl;
        std::cout << info << " Player position: (" << pkt.playerPosition[pkt.client_id].first << ", " 
                  << pkt.playerPosition[pkt.client_id].second << ")\n" << std::endl;
}

