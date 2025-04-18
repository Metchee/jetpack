#include "../shared_include/ClientGraphics.hpp"
#include <iostream>
#include <SFML/Graphics.hpp>

ClientGraphics::ClientGraphics(int windowWidth, int windowHeight)
    : _window(std::make_unique<sf::RenderWindow>(sf::VideoMode(windowWidth, windowHeight), "Jetpack")),
      _gridSize(32), // Default grid size
      _isMapLoaded(false),
      _windowWidth(windowWidth),
      _windowHeight(windowHeight)
{
    _window->setFramerateLimit(60);
    
    // Load textures
    if (!loadTextures()) {
        throw std::runtime_error("Failed to load textures");
    }
    
    // Setup sprites
    setupSprites();
}

ClientGraphics::~ClientGraphics()
{
    _window->close();
}

bool ClientGraphics::loadTextures()
{
    // Try to load required textures
    if (!_textures["player1"].loadFromFile("assets/player1.png")) {
        return false;
    }
    
    if (!_textures["player2"].loadFromFile("assets/player2.png")) {
        return false;
    }
    
    if (!_textures["coin"].loadFromFile("assets/coin.png")) {
        return false;
    }
    
    if (!_textures["wall"].loadFromFile("assets/wall.png")) {
        return false;
    }
    
    if (!_textures["electric"].loadFromFile("assets/electric.png")) {
        return false;
    }
    
    if (!_textures["finish"].loadFromFile("assets/finish.png")) {
        return false;
    }
    
    if (!_textures["background"].loadFromFile("assets/background.png")) {
        return false;
    }
    
    return true;
}

void ClientGraphics::setupSprites()
{
    // Initialize player sprites
    _sprites["player1"].setTexture(_textures["player1"]);
    _sprites["player2"].setTexture(_textures["player2"]);
    
    // Initialize tile sprites
    _sprites["coin"].setTexture(_textures["coin"]);
    _sprites["wall"].setTexture(_textures["wall"]);
    _sprites["electric"].setTexture(_textures["electric"]);
    _sprites["finish"].setTexture(_textures["finish"]);
    _sprites["background"].setTexture(_textures["background"]);
    
    // Scale sprites to grid size
    for (auto& [name, sprite] : _sprites) {
        sf::Vector2u textureSize = sprite.getTexture()->getSize();
        sprite.setScale(
            static_cast<float>(_gridSize) / textureSize.x,
            static_cast<float>(_gridSize) / textureSize.y
        );
    }
}

void ClientGraphics::loadMap(const Map& map)
{
    _map = map;
    _isMapLoaded = true;
    
    // Resize view to fit map
    float viewWidth = map.getWidth() * _gridSize;
    float viewHeight = map.getHeight() * _gridSize;
    
    _view.reset(sf::FloatRect(0, 0, viewWidth, viewHeight));
    _window->setView(_view);
}

Direction ClientGraphics::processEvents()
{
    Direction dir = Direction::NONE;
    
    sf::Event event;
    while (_window->pollEvent(event)) {
        if (event.type == sf::Event::Closed) {
            _window->close();
        }
    }
    
    // Process keyboard input
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up) || sf::Keyboard::isKeyPressed(sf::Keyboard::Space)) {
        dir = Direction::UP;
    } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down)) {
        dir = Direction::DOWN;
    }
    
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left)) {
        dir = Direction::LEFT;
    } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) {
        dir = Direction::RIGHT;
    }
    
    return dir;
}

void ClientGraphics::render(const GameState& gameState)
{
    if (!_isMapLoaded || !_window->isOpen()) {
        return;
    }
    
    _window->clear(sf::Color::Black);
    
    // Draw background
    sf::Sprite bgSprite = _sprites["background"];
    for (int y = 0; y < _map.getHeight(); y++) {
        for (int x = 0; x < _map.getWidth(); x++) {
            bgSprite.setPosition(x * _gridSize, y * _gridSize);
            _window->draw(bgSprite);
        }
    }
    
    // Draw map tiles
    for (int y = 0; y < _map.getHeight(); y++) {
        for (int x = 0; x < _map.getWidth(); x++) {
            Cell cell = _map.getCellAt(x, y);
            
            sf::Sprite* sprite = nullptr;
            
            switch (cell) {
                case Cell::WALL:
                    sprite = &_sprites["wall"];
                    break;
                case Cell::COIN:
                    sprite = &_sprites["coin"];
                    break;
                case Cell::ELECTRIC:
                    sprite = &_sprites["electric"];
                    break;
                case Cell::FINISH:
                    sprite = &_sprites["finish"];
                    break;
                default:
                    sprite = nullptr;
                    break;
            }
            
            if (sprite) {
                sprite->setPosition(x * _gridSize, y * _gridSize);
                _window->draw(*sprite);
            }
        }
    }
    
    // Draw players
    for (int i = 0; i < gameState.nbPlayers; i++) {
        if (gameState.playerState[i] != PacketModule::PLAYING) {
            continue;
        }
        
        sf::Sprite playerSprite;
        if (i == 0) {
            playerSprite = _sprites["player1"];
        } else {
            playerSprite = _sprites["player2"];
        }
        
        int x = gameState.playerPositions[i].first;
        int y = gameState.playerPositions[i].second;
        
        playerSprite.setPosition(x * _gridSize, y * _gridSize);
        _window->draw(playerSprite);
    }
    
    // Draw scores
    sf::Font font;
    if (font.loadFromFile("assets/font.ttf")) {
        for (int i = 0; i < gameState.nbPlayers; i++) {
            sf::Text scoreText;
            scoreText.setFont(font);
            scoreText.setString("Player " + std::to_string(i + 1) + ": " + std::to_string(gameState.playerScores[i]));
            scoreText.setCharacterSize(16);
            scoreText.setFillColor(sf::Color::White);
            scoreText.setPosition(10, 10 + i * 20);
            _window->draw(scoreText);
        }
    }
    
    // Draw game over message if needed
    if (gameState.gameOver) {
        sf::Text gameOverText;
        if (font.loadFromFile("assets/font.ttf")) {
            gameOverText.setFont(font);
            gameOverText.setString("Game Over!");
            gameOverText.setCharacterSize(32);
            gameOverText.setFillColor(sf::Color::Red);
            
            sf::FloatRect textBounds = gameOverText.getLocalBounds();
            gameOverText.setPosition(
                (_window->getSize().x - textBounds.width) / 2,
                (_window->getSize().y - textBounds.height) / 2
            );
            
            _window->draw(gameOverText);
        }
    }
    
    _window->display();
}

bool ClientGraphics::isWindowOpen() const
{
    return _window->isOpen();
}

void ClientGraphics::centerViewOnPlayer(const std::pair<int, int>& playerPos)
{
    float centerX = playerPos.first * _gridSize;
    float centerY = playerPos.second * _gridSize;
    
    // Keep view within map bounds
    float viewWidth = _view.getSize().x;
    float viewHeight = _view.getSize().y;
    
    float mapWidth = _map.getWidth() * _gridSize;
    float mapHeight = _map.getHeight() * _gridSize;
    
    // Adjust center to keep view within map
    if (centerX < viewWidth / 2) {
        centerX = viewWidth / 2;
    } else if (centerX > mapWidth - viewWidth / 2) {
        centerX = mapWidth - viewWidth / 2;
    }
    
    if (centerY < viewHeight / 2) {
        centerY = viewHeight / 2;
    } else if (centerY > mapHeight - viewHeight / 2) {
        centerY = mapHeight - viewHeight / 2;
    }
    
    _view.setCenter(centerX, centerY);
    _window->setView(_view);
}