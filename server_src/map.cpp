#include "../shared_include/Map.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

Map::Map() : width(0), height(0) {}

Map::~Map() {}

bool Map::loadFromFile(const std::string &filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open map file: " << filename << std::endl;
        return false;
    }

    std::string line;
    // First line contains dimensions: width height
    if (std::getline(file, line)) {
        std::istringstream iss(line);
        if (!(iss >> width >> height)) {
            std::cerr << "Invalid map dimensions in file: " << filename << std::endl;
            return false;
        }
    } else {
        std::cerr << "Failed to read dimensions from map file: " << filename << std::endl;
        return false;
    }

    // Initialize map grid
    grid.resize(height, std::vector<Cell>(width, Cell::EMPTY));
    int y = 0;

    // Read the map data
    while (std::getline(file, line) && y < height) {
        for (size_t x = 0; x < line.length() && x < width; x++) {
            switch (line[x]) {
                case '#': // Wall
                    grid[y][x] = Cell::WALL;
                    break;
                case '.': // Empty space
                    grid[y][x] = Cell::EMPTY;
                    break;
                case '$': // Coin
                    grid[y][x] = Cell::COIN;
                    coins.push_back({static_cast<int>(x), y});
                    break;
                case 'E': // Electric obstacle
                    grid[y][x] = Cell::ELECTRIC;
                    obstacles.push_back({static_cast<int>(x), y});
                    break;
                case 'S': // Start position for player 1
                    grid[y][x] = Cell::EMPTY;
                    startPositions.push_back({static_cast<int>(x), y});
                    break;
                case 's': // Start position for player 2
                    grid[y][x] = Cell::EMPTY;
                    startPositions.push_back({static_cast<int>(x), y});
                    break;
                case 'F': // Finish line
                    grid[y][x] = Cell::FINISH;
                    finishPosition = {static_cast<int>(x), y};
                    break;
                default:
                    grid[y][x] = Cell::EMPTY;
                    break;
            }
        }
        y++;
    }

    // Ensure we have at least two start positions
    while (startPositions.size() < 2) {
        startPositions.push_back({0, 0});
    }

    return true;
}

Cell Map::getCellAt(int x, int y) const {
    if (x < 0 || x >= width || y < 0 || y >= height) {
        return Cell::WALL; // Out of bounds is considered a wall
    }
    return grid[y][x];
}

bool Map::isCellWalkable(int x, int y) const {
    return getCellAt(x, y) != Cell::WALL;
}

bool Map::isCellCoin(int x, int y) const {
    return getCellAt(x, y) == Cell::COIN;
}

bool Map::isCellElectric(int x, int y) const {
    return getCellAt(x, y) == Cell::ELECTRIC;
}

bool Map::isCellFinish(int x, int y) const {
    return getCellAt(x, y) == Cell::FINISH;
}

std::pair<int, int> Map::getStartPosition(int playerIndex) const {
    if (playerIndex < 0 || playerIndex >= static_cast<int>(startPositions.size())) {
        return {0, 0}; // Default position if invalid index
    }
    return startPositions[playerIndex];
}

std::pair<int, int> Map::getFinishPosition() const {
    return finishPosition;
}

int Map::getWidth() const {
    return width;
}

int Map::getHeight() const {
    return height;
}

const std::vector<std::pair<int, int>>& Map::getCoins() const {
    return coins;
}

const std::vector<std::pair<int, int>>& Map::getObstacles() const {
    return obstacles;
}

std::string Map::serialize() const {
    std::stringstream ss;
    ss << width << " " << height << "\n";
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            switch (grid[y][x]) {
                case Cell::WALL:
                    ss << '#';
                    break;
                case Cell::EMPTY:
                    // Check if this is a start or finish position
                    if (finishPosition.first == x && finishPosition.second == y) {
                        ss << 'F';
                    } else {
                        bool isStart = false;
                        for (size_t i = 0; i < startPositions.size(); i++) {
                            if (startPositions[i].first == x && startPositions[i].second == y) {
                                ss << (i == 0 ? 'S' : 's');
                                isStart = true;
                                break;
                            }
                        }
                        if (!isStart) {
                            ss << '.';
                        }
                    }
                    break;
                case Cell::COIN:
                    ss << '$';
                    break;
                case Cell::ELECTRIC:
                    ss << 'E';
                    break;
                case Cell::FINISH:
                    ss << 'F';
                    break;
                default:
                    ss << '.';
                    break;
            }
        }
        ss << "\n";
    }
    
    return ss.str();
}

bool Map::deserialize(const std::string &data) {
    std::istringstream iss(data);
    std::string line;
    
    // First line contains dimensions
    if (std::getline(iss, line)) {
        std::istringstream dimStream(line);
        if (!(dimStream >> width >> height)) {
            return false;
        }
    } else {
        return false;
    }
    
    // Initialize map grid
    grid.resize(height, std::vector<Cell>(width, Cell::EMPTY));
    coins.clear();
    obstacles.clear();
    startPositions.clear();
    
    int y = 0;
    while (std::getline(iss, line) && y < height) {
        for (size_t x = 0; x < line.length() && x < width; x++) {
            switch (line[x]) {
                case '#':
                    grid[y][x] = Cell::WALL;
                    break;
                case '.':
                    grid[y][x] = Cell::EMPTY;
                    break;
                case '$':
                    grid[y][x] = Cell::COIN;
                    coins.push_back({static_cast<int>(x), y});
                    break;
                case 'E':
                    grid[y][x] = Cell::ELECTRIC;
                    obstacles.push_back({static_cast<int>(x), y});
                    break;
                case 'S':
                    grid[y][x] = Cell::EMPTY;
                    startPositions.push_back({static_cast<int>(x), y});
                    break;
                case 's':
                    grid[y][x] = Cell::EMPTY;
                    startPositions.push_back({static_cast<int>(x), y});
                    break;
                case 'F':
                    grid[y][x] = Cell::FINISH;
                    finishPosition = {static_cast<int>(x), y};
                    break;
                default:
                    grid[y][x] = Cell::EMPTY;
                    break;
            }
        }
        y++;
    }
    
    // Ensure we have at least two start positions
    while (startPositions.size() < 2) {
        startPositions.push_back({0, 0});
    }
    
    return true;
}