#include "../shared_include/Game.hpp"
#include <algorithm>
#include <iostream>

Player::Player(int id, const std::pair<int, int>& startPos)
    : id(id), position(startPos), score(0), dead(false), finished(false), velocity(2.0f), yVelocity(0.0f)
{
}

Player::~Player() {}

void Player::update(float deltaTime, Direction dir, const Map& map)
{
    if (dead || finished) return;

    int newX = position.first;
    int newY = position.second;

    // Apply gravity
    yVelocity += GRAVITY * deltaTime;
    
    // Apply jet pack (jump) when moving up
    if (dir == Direction::UP) {
        yVelocity = JUMP_FORCE;
    }
    
    // Update position based on vertical velocity
    newY += static_cast<int>(yVelocity * deltaTime * 10.0f);

    // Handle horizontal movement
    if (dir == Direction::LEFT) {
        newX -= static_cast<int>(velocity * deltaTime * 10.0f);
    } else if (dir == Direction::RIGHT) {
        newX += static_cast<int>(velocity * deltaTime * 10.0f);
    }

    // Make sure player stays within map bounds
    if (newX < 0) newX = 0;
    if (newX >= map.getWidth()) newX = map.getWidth() - 1;
    
    // Enforce map boundaries vertically
    if (newY < 0) {
        newY = 0;
        yVelocity = 0;
    }
    if (newY >= map.getHeight()) {
        newY = map.getHeight() - 1;
        yVelocity = 0;
    }

    // Check for collisions with walls
    if (!map.isCellWalkable(newX, newY)) {
        // If there's a horizontal collision, stop horizontal movement
        if (newX != position.first && map.isCellWalkable(position.first, newY)) {
            newX = position.first;
        }
        
        // If there's a vertical collision, stop vertical movement
        if (newY != position.second && map.isCellWalkable(newX, position.second)) {
            newY = position.second;
            yVelocity = 0;
        }
    }

    // Check for collisions with electric obstacles
    if (map.isCellElectric(newX, newY)) {
        dead = true;
    }
    
    // Check for finish line
    if (map.isCellFinish(newX, newY)) {
        finished = true;
    }

    // Update position
    position = {newX, newY};
}

void Player::collectCoin()
{
    score++;
}

bool Player::isColliding(int x, int y) const
{
    return position.first == x && position.second == y;
}

bool Player::isFinished() const
{
    return finished;
}

int Player::getId() const
{
    return id;
}

int Player::getScore() const
{
    return score;
}

std::pair<int, int> Player::getPosition() const
{
    return position;
}

void Player::setPosition(const std::pair<int, int>& pos)
{
    position = pos;
}

void Player::setDead()
{
    dead = true;
}

bool Player::isDead() const
{
    return dead;
}

// Game class implementation

Game::Game() : gameStarted(false), gameOver(false)
{
}

Game::~Game()
{
}

bool Game::loadMap(const std::string& mapFile)
{
    return map.loadFromFile(mapFile);
}

const Map& Game::getMap() const
{
    return map;
}

void Game::addPlayer(int playerId)
{
    if (players.find(playerId) != players.end()) {
        return; // Player already exists
    }
    
    // Get appropriate start position
    int playerIndex = players.size();
    std::pair<int, int> startPos = map.getStartPosition(playerIndex);
    
    players[playerId] = std::make_unique<Player>(playerId, startPos);
}

void Game::removePlayer(int playerId)
{
    players.erase(playerId);
    
    // If we don't have enough players, end the game
    if (players.size() < 2 && gameStarted && !gameOver) {
        endGame();
    }
}

void Game::update(float deltaTime)
{
    if (!gameStarted || gameOver) {
        return;
    }

    // Check if there's a winner
    bool allDead = true;
    Player* lastAlivePlayer = nullptr;
    
    for (auto& [id, player] : players) {
        // Skip update for dead or finished players
        if (!player->isDead()) {
            allDead = false;
            lastAlivePlayer = player.get();
            
            // Check for coin collection
            for (const auto& coin : map.getCoins()) {
                if (player->isColliding(coin.first, coin.second)) {
                    player->collectCoin();
                    collectedCoins.push_back(coin);
                }
            }
        }
    }
    
    // Check if all players are dead except one
    if (allDead) {
        endGame();
    } else if (lastAlivePlayer && players.size() == 1) {
        endGame();
    }
    
    // Check if all players have reached the finish line
    bool allFinished = !players.empty();
    Player* highestScorer = nullptr;
    int highestScore = -1;
    
    for (const auto& [id, player] : players) {
        if (!player->isFinished() && !player->isDead()) {
            allFinished = false;
            break;
        }
        
        if (player->getScore() > highestScore) {
            highestScore = player->getScore();
            highestScorer = player.get();
        }
    }
    
    if (allFinished) {
        endGame();
    }
}

void Game::handlePlayerInput(int playerId, Direction dir)
{
    auto it = players.find(playerId);
    if (it == players.end()) {
        return;
    }
    
    Player* player = it->second.get();
    player->update(0.016f, dir, map); // Assume fixed 60 FPS for input
}

Player* Game::getPlayer(int playerId)
{
    auto it = players.find(playerId);
    if (it == players.end()) {
        return nullptr;
    }
    return it->second.get();
}

std::vector<Player*> Game::getPlayers()
{
    std::vector<Player*> result;
    for (auto& [id, player] : players) {
        result.push_back(player.get());
    }
    return result;
}

bool Game::isGameStarted() const
{
    return gameStarted;
}

bool Game::isGameOver() const
{
    return gameOver;
}

void Game::startGame()
{
    if (players.size() < 2) {
        return; // Need at least 2 players
    }
    
    gameStarted = true;
    gameOver = false;
    gameStartTime = std::chrono::system_clock::now();
}

void Game::endGame()
{
    gameOver = true;
}

void Game::updateFromPacket(const PacketModule& packet)
{
    const auto& pkt = packet.getPacket();
    
    // Update game states
    for (int i = 0; i < pkt.nb_client; i++) {
        int clientId = i;
        
        if (pkt.playerState[i] == PacketModule::WAITING) {
            // Add player if not exists
            if (players.find(clientId) == players.end()) {
                addPlayer(clientId);
            }
        } else if (pkt.playerState[i] == PacketModule::PLAYING) {
            // Update player position
            if (players.find(clientId) != players.end()) {
                players[clientId]->setPosition(pkt.playerPosition[i]);
            }
        } else if (pkt.playerState[i] == PacketModule::ENDED) {
            // Mark player as finished or dead
            if (players.find(clientId) != players.end()) {
                players[clientId]->setDead();
            }
        }
    }
}

void Game::fillPacket(PacketModule& packet)
{
    auto& pkt = packet.getPacket();
    
    // Fill packet with game state
    pkt.nb_client = players.size();
    
    int i = 0;
    for (const auto& [id, player] : players) {
        if (i >= MAX_CLIENTS) break;
        
        pkt.playerPosition[i] = player->getPosition();
        
        if (player->isDead() || player->isFinished()) {
            pkt.playerState[i] = PacketModule::ENDED;
        } else if (gameStarted) {
            pkt.playerState[i] = PacketModule::PLAYING;
        } else {
            pkt.playerState[i] = PacketModule::WAITING;
        }
        
        i++;
    }
}