#pragma once

#include "Map.hpp"
#include "Packet.hpp"
#include <vector>
#include <memory>
#include <utility>
#include <unordered_map>
#include <chrono>

// Player movement directions
enum class Direction {
    NONE,
    UP,
    DOWN,
    LEFT,
    RIGHT
};

// Player state in the game
class Player {
public:
    Player(int id, const std::pair<int, int>& startPos);
    ~Player();

    void update(float deltaTime, Direction dir, const Map& map);
    void collectCoin();
    bool isColliding(int x, int y) const;
    bool isFinished() const;

    int getId() const;
    int getScore() const;
    std::pair<int, int> getPosition() const;
    void setPosition(const std::pair<int, int>& pos);
    void setDead();
    bool isDead() const;

private:
    int id;
    std::pair<int, int> position;
    int score;
    bool dead;
    bool finished;
    float velocity;
    const float GRAVITY = 0.2f;
    const float JUMP_FORCE = -0.5f;
    float yVelocity;
};

// Main game class
class Game {
public:
    Game();
    ~Game();

    bool loadMap(const std::string& mapFile);
    const Map& getMap() const;
    
    void addPlayer(int playerId);
    void removePlayer(int playerId);
    
    void update(float deltaTime);
    void handlePlayerInput(int playerId, Direction dir);
    
    Player* getPlayer(int playerId);
    std::vector<Player*> getPlayers();
    
    bool isGameStarted() const;
    bool isGameOver() const;
    void startGame();
    void endGame();
    
    // Update game state based on packet
    void updateFromPacket(const PacketModule& packet);
    // Create packet from current game state
    void fillPacket(PacketModule& packet);

private:
    Map map;
    std::unordered_map<int, std::unique_ptr<Player>> players;
    std::vector<std::pair<int, int>> collectedCoins;
    bool gameStarted;
    bool gameOver;
    std::chrono::time_point<std::chrono::system_clock> gameStartTime;
};