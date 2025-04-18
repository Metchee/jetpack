#include "../shared_include/Client.hpp"
#include "../shared_include/ClientGraphics.hpp"
#include "../shared_include/Map.hpp"
#include <cstdio>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <fstream>

void ClientModule::Client::receiveMapData()
{
    // Receive map size
    uint32_t mapSize;
    ssize_t bytes_read = recv(fd, &mapSize, sizeof(mapSize), 0);
    
    if (bytes_read <= 0 || bytes_read != sizeof(mapSize)) {
        throw std::runtime_error("Failed to receive map size from server");
    }
    
    if (debugMode) {
        std::cout << "Receiving map data (size: " << mapSize << " bytes)" << std::endl;
    }
    
    // Receive map data
    std::vector<char> mapData(mapSize);
    size_t totalRead = 0;
    
    while (totalRead < mapSize) {
        bytes_read = recv(fd, mapData.data() + totalRead, mapSize - totalRead, 0);
        
        if (bytes_read <= 0) {
            throw std::runtime_error("Failed to receive map data from server");
        }
        
        totalRead += bytes_read;
    }
    
    // Parse map data
    std::string mapString(mapData.begin(), mapData.end());
    
    {
        std::lock_guard<std::mutex> lock(_mapMutex);
        if (!_map.deserialize(mapString)) {
            throw std::runtime_error("Failed to parse map data");
        }
        
        _mapLoaded = true;
        _mapCV.notify_all();
    }
    
    if (debugMode) {
        std::cout << "Map loaded successfully (" << _map.getWidth() << "x" << _map.getHeight() << ")" << std::endl;
    }
}