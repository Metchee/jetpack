#include "../shared_include/Client.hpp"
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

namespace ClientModule {

// Function to ensure the assets directory exists
bool ensureAssetsDirectory() {
    // Check if assets directory exists, create if it doesn't
    struct stat st;
    if (stat("assets", &st) != 0) {
        // Directory doesn't exist, create it
        if (mkdir("assets", 0755) != 0) {
            std::cerr << "Failed to create assets directory: " << strerror(errno) << std::endl;
            return false;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        // Path exists but isn't a directory
        std::cerr << "assets exists but is not a directory" << std::endl;
        return false;
    }
    
    return true;
}

// Function to save a map file from packet data
bool saveMapFromPacket(const PacketModule& packet, const std::string& filename) {
    if (!ensureAssetsDirectory()) {
        return false;
    }
    
    std::string fullPath = "assets/" + filename;
    std::ofstream file(fullPath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to create map file: " << fullPath << std::endl;
        return false;
    }
    
    // Write the map data to the file
    file.write(packet.getPacket().map, strlen(packet.getPacket().map));
    
    if (!file) {
        std::cerr << "Error writing to map file: " << fullPath << std::endl;
        return false;
    }
    
    file.close();
    std::cout << "Map saved to: " << fullPath << std::endl;
    return true;
}

// Function to check if a file exists
bool fileExists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

// Function to verify all required assets exist
bool verifyRequiredAssets() {
    std::vector<std::string> requiredAssets = {
        "assets/player_sprite_sheet.png",
        "assets/coins_sprite_sheet.png",
        "assets/zapper_sprite_sheet.png",
        "assets/background.png",
        "assets/jetpack_font.ttf",
        "assets/jetpack_lp.wav",
        "assets/coin_pickup_1.wav",
        "assets/dud_zapper_pop.wav",
        "assets/theme.ogg"
    };
    
    bool allAssetsExist = true;
    for (const auto& asset : requiredAssets) {
        if (!fileExists(asset)) {
            std::cerr << "Missing required asset: " << asset << std::endl;
            allAssetsExist = false;
        }
    }
    
    return allAssetsExist;
}

} // namespace ClientModule