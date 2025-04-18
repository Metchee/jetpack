#pragma once

#include <memory>
#include <unordered_map>
#include <string>
#include <SFML/Graphics.hpp>
#include "Map.hpp"
#include "Game.hpp"
#include "Packet.hpp"

// Game state structure for rendering
struct GameState {
    int nbPlayers;
    int clientId;
    std::vector<std::pair<int, int>> playerPositions;
    std::vector<PacketModule::gameState> playerState;
    std::vector<int> playerScores;
    bool gameStarted;
    bool gameOver;
};

class ClientGraphics {
public:
    ClientGraphics(int windowWidth = 800, int windowHeight = 600);
    ~ClientGraphics();
    
    void loadMap(const Map& map);
    Direction processEvents();
    void render(const GameState& gameState);
    bool isWindowOpen() const;
    void centerViewOnPlayer(const std::pair<int, int>& playerPos);
    
private:
    bool loadTextures();
    void setupSprites();
    
    std::unique_ptr<sf::RenderWindow> _window;
    sf::View _view;
    
    std::unordered_map<std::string, sf::Texture> _textures;
    std::unordered_map<std::string, sf::Sprite> _sprites;
    
    Map _map;
    int _gridSize;
    bool _isMapLoaded;
    
    int _windowWidth;
    int _windowHeight;
};