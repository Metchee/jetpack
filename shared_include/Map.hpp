#pragma once

#include <vector>
#include <string>
#include <utility>

enum class Cell {
    EMPTY,
    WALL,
    COIN,
    ELECTRIC,
    FINISH
};

class Map {
public:
    Map();
    ~Map();

    bool loadFromFile(const std::string &filename);
    
    Cell getCellAt(int x, int y) const;
    bool isCellWalkable(int x, int y) const;
    bool isCellCoin(int x, int y) const;
    bool isCellElectric(int x, int y) const;
    bool isCellFinish(int x, int y) const;

    std::pair<int, int> getStartPosition(int playerIndex) const;
    std::pair<int, int> getFinishPosition() const;
    
    int getWidth() const;
    int getHeight() const;
    
    const std::vector<std::pair<int, int>>& getCoins() const;
    const std::vector<std::pair<int, int>>& getObstacles() const;
    
    // Serialization for network transfer
    std::string serialize() const;
    bool deserialize(const std::string &data);

private:
    int width;
    int height;
    std::vector<std::vector<Cell>> grid;
    std::vector<std::pair<int, int>> startPositions;
    std::vector<std::pair<int, int>> coins;
    std::vector<std::pair<int, int>> obstacles;
    std::pair<int, int> finishPosition;
};