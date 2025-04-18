#include "../shared_include/Game.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>

namespace ClientModule {

Game::Game(int clientId)
    : _running(true), _clientId(clientId), _jetpackActive(false),
      _tileSize(32), _mapWidth(0), _mapHeight(0), _score(0),
      _gameState(PacketModule::WAITING) {
}

Game::~Game() {
    if (_backgroundMusic.getStatus() == sf::Music::Playing) {
        _backgroundMusic.stop();
    }
}

void Game::init() {
    loadAssets();
}

void Game::loadAssets() {
    // Load textures
    if (!_playerTexture.loadFromFile("assets/player_sprite_sheet.png")) {
        std::cerr << "Failed to load player texture" << std::endl;
    }
    
    if (!_coinTexture.loadFromFile("assets/coins_sprite_sheet.png")) {
        std::cerr << "Failed to load coin texture" << std::endl;
    }
    
    if (!_electricTexture.loadFromFile("assets/zapper_sprite_sheet.png")) {
        std::cerr << "Failed to load electric texture" << std::endl;
    }
    
    if (!_backgroundTexture.loadFromFile("assets/background.png")) {
        std::cerr << "Failed to load background texture" << std::endl;
    }
    
    // Load font
    if (!_font.loadFromFile("assets/jetpack_font.ttf")) {
        std::cerr << "Failed to load font" << std::endl;
    }
    
    // Setup text
    _scoreText.setFont(_font);
    _scoreText.setCharacterSize(24);
    _scoreText.setFillColor(sf::Color::White);
    _scoreText.setPosition(10, 10);
    
    _stateText.setFont(_font);
    _stateText.setCharacterSize(36);
    _stateText.setFillColor(sf::Color::White);
    _stateText.setPosition(400, 300);
    
    // Setup background
    _backgroundSprite.setTexture(_backgroundTexture);
    
    // Load sounds
    if (!_jetpackSoundBuffer.loadFromFile("assets/jetpack_lp.wav")) {
        std::cerr << "Failed to load jetpack sound" << std::endl;
    }
    _jetpackSound.setBuffer(_jetpackSoundBuffer);
    _jetpackSound.setLoop(true);
    
    if (!_coinSoundBuffer.loadFromFile("assets/coin_pickup_1.wav")) {
        std::cerr << "Failed to load coin sound" << std::endl;
    }
    _coinSound.setBuffer(_coinSoundBuffer);
    
    if (!_electricSoundBuffer.loadFromFile("assets/dud_zapper_pop.wav")) {
        std::cerr << "Failed to load electric sound" << std::endl;
    }
    _electricSound.setBuffer(_electricSoundBuffer);
    
    // Load music
    if (!_backgroundMusic.openFromFile("assets/theme.ogg")) {
        std::cerr << "Failed to load background music" << std::endl;
    }
    _backgroundMusic.setLoop(true);
    _backgroundMusic.setVolume(50.f);
    _backgroundMusic.play();
    
    // Initialize player for this client
    _players[_clientId].id = _clientId;
    _players[_clientId].position = std::make_pair(0, 0);
    _players[_clientId].isJetpackActive = false;
    _players[_clientId].currentFrame = 0;
    _players[_clientId].isDead = false;
    
    // Setup player sprite
    _players[_clientId].sprite.setTexture(_playerTexture);
    _players[_clientId].sprite.setTextureRect(sf::IntRect(0, 0, 32, 32));
    _players[_clientId].sprite.setPosition(0, 0);
}

void Game::parseMap(const char* mapData) {
    if (!mapData || strlen(mapData) == 0) {
        std::cerr << "Empty map data!" << std::endl;
        return;
    }
    
    std::istringstream stream(mapData);
    std::string line;
    _map.clear();
    int y = 0;
    
    while (std::getline(stream, line)) {
        std::vector<MapTile> row;
        for (size_t x = 0; x < line.length(); ++x) {
            MapTile tile;
            switch (line[x]) {
                case '#':
                    tile.type = TileType::WALL;
                    break;
                case 'C':
                    tile.type = TileType::COIN;
                    tile.sprite.setTexture(_coinTexture);
                    tile.sprite.setTextureRect(sf::IntRect(0, 0, _tileSize, _tileSize));
                    break;
                case 'E':
                    tile.type = TileType::ELECTRIC;
                    tile.sprite.setTexture(_electricTexture);
                    tile.sprite.setTextureRect(sf::IntRect(0, 0, _tileSize, _tileSize));
                    break;
                case 'F':
                    tile.type = TileType::END;
                    break;
                default:
                    tile.type = TileType::EMPTY;
                    break;
            }
            tile.sprite.setPosition(x * _tileSize, y * _tileSize);
            row.push_back(tile);
        }
        _map.push_back(row);
        ++y;
    }
    
    _mapWidth = _map[0].size();
    _mapHeight = _map.size();
}

void Game::processInput() {
    bool newJetpackState = sf::Keyboard::isKeyPressed(sf::Keyboard::Space);
    
    if (newJetpackState != _jetpackActive) {
        setJetpackActive(newJetpackState);
    }
}

void Game::setJetpackActive(bool active) {
    _jetpackActive = active;
    
    if (_jetpackActive && _jetpackSound.getStatus() != sf::Sound::Playing) {
        _jetpackSound.play();
    } else if (!_jetpackActive && _jetpackSound.getStatus() == sf::Sound::Playing) {
        _jetpackSound.stop();
    }
}

void Game::update(float deltaTime) {
    if (_gameState != PacketModule::PLAYING) {
        return;
    }
    
    // Update player animation
    for (auto& playerPair : _players) {
        Player& player = playerPair.second;
        if (player.animationClock.getElapsedTime().asSeconds() > 0.1f) {
            player.currentFrame = (player.currentFrame + 1) % 4;
            player.sprite.setTextureRect(sf::IntRect(player.currentFrame * _tileSize, 0, _tileSize, _tileSize));
            player.animationClock.restart();
        }
    }
    
    // Update coin animations
    for (auto& row : _map) {
        for (auto& tile : row) {
            if (tile.type == TileType::COIN) {
                // Animate coins
                static sf::Clock coinAnimClock;
                static int coinFrame = 0;
                
                if (coinAnimClock.getElapsedTime().asSeconds() > 0.2f) {
                    coinFrame = (coinFrame + 1) % 6;
                    for (auto& r : _map) {
                        for (auto& t : r) {
                            if (t.type == TileType::COIN) {
                                t.sprite.setTextureRect(sf::IntRect(coinFrame * _tileSize, 0, _tileSize, _tileSize));
                            }
                        }
                    }
                    coinAnimClock.restart();
                }
                break;
            }
        }
    }
    
    // Update electric tile animations
    for (auto& row : _map) {
        for (auto& tile : row) {
            if (tile.type == TileType::ELECTRIC) {
                // Animate electric tiles
                static sf::Clock electricAnimClock;
                static int electricFrame = 0;
                
                if (electricAnimClock.getElapsedTime().asSeconds() > 0.1f) {
                    electricFrame = (electricFrame + 1) % 4;
                    for (auto& r : _map) {
                        for (auto& t : r) {
                            if (t.type == TileType::ELECTRIC) {
                                t.sprite.setTextureRect(sf::IntRect(electricFrame * _tileSize, 0, _tileSize, _tileSize));
                            }
                        }
                    }
                    electricAnimClock.restart();
                }
                break;
            }
        }
    }
}

void Game::render(sf::RenderWindow& window) {
    window.clear();
    
    // Draw background
    window.draw(_backgroundSprite);
    
    // Draw map
    for (const auto& row : _map) {
        for (const auto& tile : row) {
            if (tile.type != TileType::EMPTY && tile.type != TileType::WALL) {
                window.draw(tile.sprite);
            }
        }
    }
    
    // Draw players
    for (const auto& playerPair : _players) {
        window.draw(playerPair.second.sprite);
    }
    
    // Draw UI
    _scoreText.setString("Score: " + std::to_string(_score) + " Player: " + std::to_string(_clientId));
    window.draw(_scoreText);
    
    // Draw game state if not playing
    if (_gameState != PacketModule::PLAYING) {
        std::string stateText;
        switch (_gameState) {
            case PacketModule::WAITING:
                stateText = "Waiting for players...";
                break;
            case PacketModule::ENDED:
                stateText = "Game Over! Score: " + std::to_string(_score);
                break;
            default:
                break;
        }
        _stateText.setString(stateText);
        sf::FloatRect textBounds = _stateText.getLocalBounds();
        _stateText.setOrigin(textBounds.width / 2, textBounds.height / 2);
        _stateText.setPosition(window.getSize().x / 2, window.getSize().y / 2);
        window.draw(_stateText);
    }
    
    window.display();
}

void Game::updateGameState(const PacketModule& packet) {
    std::lock_guard<std::mutex> lock(_stateMutex);
    
    // Update game state
    _gameState = packet.getstate();
    
    // Update players
    for (int i = 0; i < packet.getNbClient(); ++i) {
        const auto& playerPacket = packet.getPacket();
        
        if (_players.find(i) == _players.end()) {
            // Create new player
            Player newPlayer;
            newPlayer.id = i;
            newPlayer.position = playerPacket.playerPosition[i];
            newPlayer.sprite.setTexture(_playerTexture);
            newPlayer.sprite.setTextureRect(sf::IntRect(0, 0, _tileSize, _tileSize));
            newPlayer.currentFrame = 0;
            newPlayer.isDead = false;
            _players[i] = newPlayer;
        }
        
        // Update existing player
        _players[i].position = playerPacket.playerPosition[i];
        _players[i].sprite.setPosition(playerPacket.playerPosition[i].first, 
                                     playerPacket.playerPosition[i].second);
        
        // Check if it's this client's player
        if (i == _clientId) {
            // Check for coin collisions or death
            int tileX = playerPacket.playerPosition[i].first / _tileSize;
            int tileY = playerPacket.playerPosition[i].second / _tileSize;
            
            if (tileX >= 0 && tileX < _mapWidth && tileY >= 0 && tileY < _mapHeight) {
                TileType tileType = _map[tileY][tileX].type;
                
                if (tileType == TileType::COIN) {
                    _coinSound.play();
                    _score++;
                    _map[tileY][tileX].type = TileType::EMPTY;
                } else if (tileType == TileType::ELECTRIC) {
                    _electricSound.play();
                    _players[i].isDead = true;
                }
            }
        }
    }
    
    // Update map if not set yet
    if (_map.empty() && strlen(packet.getPacket().map) > 0) {
        parseMap(packet.getPacket().map);
    }
}

} // namespace ClientModule