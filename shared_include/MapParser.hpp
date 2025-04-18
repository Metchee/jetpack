#pragma once

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>

// Map element types
enum class TileType {
    EMPTY = 0,
    WALL,
    COIN,
    ELECTRIC,
    END_MARKER
};

// Map structure
struct GameMap {
    int width;
    int height;
    std::vector<std::vector<TileType>> tiles;
    
    // Utility function to get tile at a specific position
    TileType getTile(int x, int y) const {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            return tiles[y][x];
        }
        return TileType::WALL;  // Out of bounds is wall
    }
    
    // Load map from file
    static GameMap loadFromFile(const std::string& filename) {
        GameMap map;
        std::ifstream file(filename);
        
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open map file: " + filename);
        }
        
        std::string line;
        int lineNum = 0;
        
        // Read map dimensions from first line
        if (std::getline(file, line)) {
            std::istringstream iss(line);
            iss >> map.width >> map.height;
            
            if (map.width <= 0 || map.height <= 0) {
                throw std::runtime_error("Invalid map dimensions");
            }
            
            // Initialize map tiles
            map.tiles.resize(map.height, std::vector<TileType>(map.width, TileType::EMPTY));
        } else {
            throw std::runtime_error("Invalid map file format");
        }
        
        // Read map data
        while (std::getline(file, line) && lineNum < map.height) {
            if (line.length() < static_cast<size_t>(map.width)) {
                throw std::runtime_error("Map line too short");
            }
            
            for (int x = 0; x < map.width; ++x) {
                char c = line[x];
                switch (c) {
                    case '#':
                        map.tiles[lineNum][x] = TileType::WALL;
                        break;
                    case 'C':
                        map.tiles[lineNum][x] = TileType::COIN;
                        break;
                    case 'E':
                        map.tiles[lineNum][x] = TileType::ELECTRIC;
                        break;
                    case 'F':
                        map.tiles[lineNum][x] = TileType::END_MARKER;
                        break;
                    case '.':
                    default:
                        map.tiles[lineNum][x] = TileType::EMPTY;
                        break;
                }
            }
            
            lineNum++;
        }
        
        if (lineNum < map.height) {
            throw std::runtime_error("Map file too short");
        }
        
        return map;
    }
    
    // Serialize map to string (for network transmission)
    std::string serialize() const {
        std::ostringstream oss;
        
        // Write dimensions
        oss << width << " " << height << "\n";
        
        // Write map data
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                switch (tiles[y][x]) {
                    case TileType::WALL:
                        oss << '#';
                        break;
                    case TileType::COIN:
                        oss << 'C';
                        break;
                    case TileType::ELECTRIC:
                        oss << 'E';
                        break;
                    case TileType::END_MARKER:
                        oss << 'F';
                        break;
                    case TileType::EMPTY:
                    default:
                        oss << '.';
                        break;
                }
            }
            oss << '\n';
        }
        
        return oss.str();
    }
    
    // Deserialize map from string (received from network)
    static GameMap deserialize(const std::string& data) {
        GameMap map;
        std::istringstream iss(data);
        std::string line;
        
        // Read dimensions
        if (std::getline(iss, line)) {
            std::istringstream dimStream(line);
            dimStream >> map.width >> map.height;
            
            if (map.width <= 0 || map.height <= 0) {
                throw std::runtime_error("Invalid map dimensions");
            }
            
            // Initialize map tiles
            map.tiles.resize(map.height, std::vector<TileType>(map.width, TileType::EMPTY));
        } else {
            throw std::runtime_error("Invalid map data format");
        }
        
        // Read map data
        int lineNum = 0;
        while (std::getline(iss, line) && lineNum < map.height) {
            if (line.length() < static_cast<size_t>(map.width)) {
                // Pad short lines with empty tiles
                line.append(map.width - line.length(), '.');
            }
            
            for (int x = 0; x < map.width; ++x) {
                if (x < static_cast<int>(line.length())) {
                    char c = line[x];
                    switch (c) {
                        case '#':
                            map.tiles[lineNum][x] = TileType::WALL;
                            break;
                        case 'C':
                            map.tiles[lineNum][x] = TileType::COIN;
                            break;
                        case 'E':
                            map.tiles[lineNum][x] = TileType::ELECTRIC;
                            break;
                        case 'F':
                            map.tiles[lineNum][x] = TileType::END_MARKER;
                            break;
                        case '.':
                        default:
                            map.tiles[lineNum][x] = TileType::EMPTY;
                            break;
                    }
                } else {
                    map.tiles[lineNum][x] = TileType::EMPTY;
                }
            }
            
            lineNum++;
        }
        
        // If we didn't get enough lines, fill the rest with empty tiles
        while (lineNum < map.height) {
            map.tiles[lineNum].assign(map.width, TileType::EMPTY);
            lineNum++;
        }
        
        return map;
    }
};

// Sample map creation utility
class MapGenerator {
public:
    // Generate a simple test map
    static GameMap generateTestMap(int width, int height) {
        GameMap map;
        map.width = width;
        map.height = height;
        map.tiles.resize(height, std::vector<TileType>(width, TileType::EMPTY));
        
        // Add walls at top and bottom
        for (int x = 0; x < width; ++x) {
            map.tiles[0][x] = TileType::WALL;
            map.tiles[height - 1][x] = TileType::WALL;
        }
        
        // Add some coins
        for (int x = 5; x < width; x += 7) {
            for (int y = 3; y < height - 3; y += 5) {
                map.tiles[y][x] = TileType::COIN;
            }
        }
        
        // Add some electric obstacles
        for (int x = 10; x < width; x += 15) {
            for (int y = 5; y < height - 5; y += 8) {
                map.tiles[y][x] = TileType::ELECTRIC;
            }
        }
        
        // Add end marker
        if (width > 10) {
            map.tiles[height / 2][width - 5] = TileType::END_MARKER;
        }
        
        return map;
    }
    
    // Save map to file
    static void saveMapToFile(const GameMap& map, const std::string& filename) {
        std::ofstream file(filename);
        
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file for writing: " + filename);
        }
        
        file << map.serialize();
        
        if (!file.good()) {
            throw std::runtime_error("Error writing to file: " + filename);
        }
    }
};