#pragma once

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>

enum class TileType {
    EMPTY = 0,
    WALL,
    COIN,
    ELECTRIC,
    END_MARKER
};

struct GameMap {
    int width;
    int height;
    std::vector<std::vector<TileType>> tiles;
    TileType getTile(int x, int y) const {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            return tiles[y][x];
        }
        return TileType::WALL;
    }
    static GameMap loadFromFile(const std::string& filename) {
        GameMap map;
        std::ifstream file(filename);
        
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open map file: " + filename);
        }
        std::string line;
        int lineNum = 0;
        if (std::getline(file, line)) {
            std::istringstream iss(line);
            iss >> map.width >> map.height;
            
            if (map.width <= 0 || map.height <= 0) {
                throw std::runtime_error("Invalid map dimensions");
            }
            map.tiles.resize(map.height, std::vector<TileType>(map.width, TileType::EMPTY));
        } else {
            throw std::runtime_error("Invalid map file format");
        }
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
                    case 'c':
                        map.tiles[lineNum][x] = TileType::COIN;
                        break;
                    case 'e':
                        map.tiles[lineNum][x] = TileType::ELECTRIC;
                        break;
                    case 'f':
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
    std::string serialize() const {
        std::ostringstream oss;
        oss << width << " " << height << "\n";
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                switch (tiles[y][x]) {
                    case TileType::WALL:
                        oss << '#';
                        break;
                    case TileType::COIN:
                        oss << 'c';
                        break;
                    case TileType::ELECTRIC:
                        oss << 'e';
                        break;
                    case TileType::END_MARKER:
                        oss << 'c';
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
    static GameMap deserialize(const std::string& data) {
        GameMap map;
        std::istringstream iss(data);
        std::string line;
        
        if (std::getline(iss, line)) {
            std::istringstream dimStream(line);
            dimStream >> map.width >> map.height;
            
            if (map.width <= 0 || map.height <= 0) {
                throw std::runtime_error("Invalid map dimensions");
            }
            map.tiles.resize(map.height, std::vector<TileType>(map.width, TileType::EMPTY));
        } else {
            throw std::runtime_error("Invalid map data format");
        }
        int lineNum = 0;
        while (std::getline(iss, line) && lineNum < map.height) {
            if (line.length() < static_cast<size_t>(map.width)) {
                line.append(map.width - line.length(), '.');
            }
            
            for (int x = 0; x < map.width; ++x) {
                if (x < static_cast<int>(line.length())) {
                    char c = line[x];
                    switch (c) {
                        case '#':
                            map.tiles[lineNum][x] = TileType::WALL;
                            break;
                        case 'c':
                            map.tiles[lineNum][x] = TileType::COIN;
                            break;
                        case 'e':
                            map.tiles[lineNum][x] = TileType::ELECTRIC;
                            break;
                        case 'f':
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
        while (lineNum < map.height) {
            map.tiles[lineNum].assign(map.width, TileType::EMPTY);
            lineNum++;
        }
        
        return map;
    }
};

class MapGenerator {
public:
    static GameMap generateTestMap(int width, int height) {
        GameMap map;
        map.width = width;
        map.height = height;
        map.tiles.resize(height, std::vector<TileType>(width, TileType::EMPTY));
        
        for (int x = 0; x < width; ++x) {
            map.tiles[0][x] = TileType::WALL;
            map.tiles[height - 1][x] = TileType::WALL;
        }
        for (int x = 5; x < width; x += 7) {
            for (int y = 3; y < height - 3; y += 5) {
                map.tiles[y][x] = TileType::COIN;
            }
        }
        for (int x = 10; x < width; x += 15) {
            for (int y = 5; y < height - 5; y += 8) {
                map.tiles[y][x] = TileType::ELECTRIC;
            }
        }
        if (width > 10) {
            map.tiles[height / 2][width - 5] = TileType::END_MARKER;
        }
        return map;
    }
    
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