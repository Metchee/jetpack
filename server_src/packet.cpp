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
    
    std::string stateStr;
    switch(pkt.playerState[pkt.client_id]) {
        case PLAYING:
            stateStr = "PLAYING";
            break;
        case WAITING:
            stateStr = "WAITING";
            break;
        case ENDED:
            stateStr = "ENDED";
            break;
        default:
            stateStr = "UNKNOWN";
    }
    
    std::cout << info << " Player state: " << stateStr << std::endl;
    std::cout << info << " Player position: (" << pkt.playerPosition[pkt.client_id].first << ", " 
              << pkt.playerPosition[pkt.client_id].second << ")\n";
    
    std::cout << info << " All player states: ";
    for (int i = 0; i < pkt.nb_client; i++) {
        std::string pStateStr;
        switch(pkt.playerState[i]) {
            case PLAYING: pStateStr = "PLAYING"; break;
            case WAITING: pStateStr = "WAITING"; break;
            case ENDED: pStateStr = "ENDED"; break;
            default: pStateStr = "UNKNOWN";
        }
        std::cout << "Player " << i << ": " << pStateStr << " ";
    }
    std::cout << std::endl;
    
    std::cout << info << " All player positions: ";
    for (int i = 0; i < pkt.nb_client; i++) {
        std::cout << "Player " << i << ": (" 
                  << pkt.playerPosition[i].first << ", " 
                  << pkt.playerPosition[i].second << ") ";
    }
    std::cout << std::endl;
    
    if (strlen(pkt.map) > 0 && strlen(pkt.map) < 20) {
        std::cout << info << " Map: " << pkt.map << std::endl;
    } else if (strlen(pkt.map) > 0) {
        std::cout << info << " Map data present (length: " << strlen(pkt.map) << ")" << std::endl;
    } else {
        std::cout << info << " No map data" << std::endl;
    }
}