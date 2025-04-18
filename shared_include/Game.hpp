#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <memory>
#include <vector>
#include <map>
#include <string>
#include <mutex>
#include "Packet.hpp"

namespace ClientModule {

enum class TileType {
    EMPTY,
    WALL,
    COIN,
    ELECTRIC,
    END
};

struct MapTile {
    TileType type;
    sf::Sprite sprite;
};

struct Player {
    int id;
    std::pair<int, int> position;
    bool isJetpackActive;
    sf::Sprite sprite;
    sf::Clock animationClock;
    int currentFrame;
    bool isDead;
};

class Game {
public:
    Game(int clientId);
    ~Game();

    void init();
    void loadAssets();
    void parseMap(const char* mapData);
    void processInput();
    void update(float deltaTime);
    void render(sf::RenderWindow& window);
    void updateGameState(const PacketModule& packet);
    void setClientId(int id) { _clientId = id; }
    bool isRunning() const { return _running; }
    void setMap(const char* mapData) { parseMap(mapData); }
    bool isJetpackActive() const { return _jetpackActive; }
    void setJetpackActive(bool active);

private:
    bool _running;
    int _clientId;
    bool _jetpackActive;
    std::vector<std::vector<MapTile>> _map;
    std::map<int, Player> _players;
    sf::Texture _playerTexture;
    sf::Texture _coinTexture;
    sf::Texture _electricTexture;
    sf::Texture _backgroundTexture;
    sf::Sprite _backgroundSprite;
    sf::Font _font;
    sf::Text _scoreText;
    sf::Text _stateText;
    sf::Music _backgroundMusic;
    sf::Sound _jetpackSound;
    sf::Sound _coinSound;
    sf::Sound _electricSound;
    sf::SoundBuffer _jetpackSoundBuffer;
    sf::SoundBuffer _coinSoundBuffer;
    sf::SoundBuffer _electricSoundBuffer;
    int _tileSize;
    int _mapWidth;
    int _mapHeight;
    int _score;
    PacketModule::gameState _gameState;
    std::mutex _stateMutex;
};

} // namespace ClientModule