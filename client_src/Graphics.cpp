#include "Graphics.hpp"
#include <iostream>

GraphicsEngine::GraphicsEngine()
    : _windowWidth(800), _windowHeight(600), _clientId(0), _nbClients(0), 
      _gameState(PacketModule::WAITING), _jetpackActive(false), _mutex(nullptr),
      _backgroundScroll(0.0f), _playerAnimFrame(0), _animClock(0.0f)
{
    _playerPositions.resize(MAX_CLIENTS, std::make_pair(0, 0));
    _playerAlive.resize(MAX_CLIENTS, true);
    _playerScores.resize(MAX_CLIENTS, 0);
}

GraphicsEngine::~GraphicsEngine()
{
    if (_window.isOpen()) {
        _window.close();
    }
}

bool GraphicsEngine::init(int windowWidth, int windowHeight, const std::string& title)
{
    _windowWidth = windowWidth;
    _windowHeight = windowHeight;
    
    // Créer la fenêtre
    _window.create(sf::VideoMode(windowWidth, windowHeight), title);
    _window.setFramerateLimit(60);
    
    // Initialiser la vue
    _gameView.setSize(windowWidth, windowHeight);
    _gameView.setCenter(windowWidth / 2, windowHeight / 2);
    
    std::cout << "Window created with dimensions: " << windowWidth << "x" << windowHeight << std::endl;
    
    return _window.isOpen();
}

bool GraphicsEngine::loadResources()
{
    std::cout << "Loading resources..." << std::endl;
    bool success = true;
    
    // Charger la texture du fond
    if (!_textures["background"].loadFromFile("assets/background.png")) {
        std::cerr << "Failed to load background texture" << std::endl;
        success = false;
    }
    
    // Charger les textures des sprites
    if (!_textures["player"].loadFromFile("assets/player_sprite_sheet.png")) {
        std::cerr << "Failed to load player texture" << std::endl;
        success = false;
    }
    
    if (!_textures["otherPlayer"].loadFromFile("assets/player_sprite_sheet.png")) {
        std::cerr << "Failed to load other player texture" << std::endl;
        success = false;
    }
    
    if (!_textures["coin"].loadFromFile("assets/coins_sprite_sheet.png")) {
        std::cerr << "Failed to load coin texture" << std::endl;
        success = false;
    }
    
    if (!_textures["electric"].loadFromFile("assets/zapper_sprite_sheet.png")) {
        std::cerr << "Failed to load electric texture" << std::endl;
        success = false;
    }
    
    if (!_textures["wall"].loadFromFile("assets/background.png")) {
        std::cerr << "Failed to load wall texture (using background as fallback)" << std::endl;
        // Still continue, we'll use a subsection of the background
    }
    
    // Charger la police
    if (!_font.loadFromFile("assets/jetpack_font.ttf")) {
        std::cerr << "Failed to load font. Trying default font..." << std::endl;
        // Try system font as fallback
        if (!_font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")) {
            std::cerr << "Failed to load fallback font" << std::endl;
            success = false;
        }
    }
    
    // Configure background
    _backgroundSprite.setTexture(_textures["background"]);
    
    // Configure player sprite with animation frames
    _playerSprite.setTexture(_textures["player"]);
    _playerSprite.setTextureRect(sf::IntRect(0, 0, 48, 48)); // First frame
    _playerSprite.setScale(1.5f, 1.5f);
    
    _otherPlayerSprite.setTexture(_textures["otherPlayer"]);
    _otherPlayerSprite.setTextureRect(sf::IntRect(0, 0, 48, 48)); // First frame
    _otherPlayerSprite.setScale(1.5f, 1.5f);
    
    // Configure other sprites
    _tileSprites[WALL].setTexture(_textures["wall"]);
    _tileSprites[WALL].setTextureRect(sf::IntRect(0, 100, 32, 32)); // Subsection of background
    
    _tileSprites[COIN].setTexture(_textures["coin"]);
    _tileSprites[COIN].setTextureRect(sf::IntRect(0, 0, 16, 16)); // First coin frame
    _tileSprites[COIN].setScale(2.0f, 2.0f);
    
    _tileSprites[ELECTRIC].setTexture(_textures["electric"]);
    _tileSprites[ELECTRIC].setTextureRect(sf::IntRect(0, 0, 16, 48)); // First electric frame
    _tileSprites[ELECTRIC].setScale(2.0f, 2.0f);
    
    // Create a red rectangle for the finish line
    sf::RectangleShape finishRect(sf::Vector2f(10, TILE_SIZE * 3));
    finishRect.setFillColor(sf::Color::Red);
    sf::Texture finishTexture;
    finishTexture.create(10, TILE_SIZE * 3);
    finishTexture.update((sf::Uint8*)finishRect.getFillColor().toInteger(), 1, 1, 0, 0);
    _textures["finish"] = finishTexture;
    _tileSprites[FINISH].setTexture(_textures["finish"]);
    
    if (success) {
        std::cout << "Resources loaded successfully" << std::endl;
    }
    
    return success;
}

void GraphicsEngine::updateGameState(const PacketModule& packet, std::mutex& mutex)
{
    std::lock_guard<std::mutex> lock(mutex);
    
    _mutex = &mutex;
    _clientId = packet.getClientId();
    _nbClients = packet.getNbClient();
    _gameState = packet.getstate();
    
    // Mettre à jour les positions et états des joueurs
    auto& pkt = packet.getPacket();
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        _playerPositions[i] = pkt.playerPosition[i];
        _playerScores[i] = pkt.playerScore[i];
        _playerAlive[i] = (pkt.playerState[i] != PacketModule::ENDED);
    }
}

void GraphicsEngine::setMap(const std::vector<std::vector<TileType>>& map)
{
    if (_mutex) {
        std::lock_guard<std::mutex> lock(*_mutex);
        _map = map;
        std::cout << "Map set with size: " << map.size() << "x" << (map.empty() ? 0 : map[0].size()) << std::endl;
    } else {
        _map = map;
        std::cout << "Map set with size: " << map.size() << "x" << (map.empty() ? 0 : map[0].size()) << " (no mutex)" << std::endl;
    }
}

bool GraphicsEngine::isOpen() const
{
    return _window.isOpen();
}

bool GraphicsEngine::getJetpackActive() const
{
    return _jetpackActive;
}

void GraphicsEngine::run()
{
    static sf::Clock frameClock;
    float deltaTime = frameClock.restart().asSeconds();
    
    handleEvents();
    updateAnimations(deltaTime);
    render();
}

void GraphicsEngine::updateAnimations(float deltaTime)
{
    // Update player animation frame
    _animClock += deltaTime;
    if (_animClock >= 0.1f) { // Change animation frame every 0.1 seconds
        _animClock = 0.0f;
        _playerAnimFrame = (_playerAnimFrame + 1) % 4; // 4 frames of animation
        
        // Update player sprite texture rect
        if (_jetpackActive) {
            // Jetpack animation (row 1)
            _playerSprite.setTextureRect(sf::IntRect(_playerAnimFrame * 48, 48, 48, 48));
            _otherPlayerSprite.setTextureRect(sf::IntRect(_playerAnimFrame * 48, 48, 48, 48));
        } else {
            // Running animation (row 0)
            _playerSprite.setTextureRect(sf::IntRect(_playerAnimFrame * 48, 0, 48, 48));
            _otherPlayerSprite.setTextureRect(sf::IntRect(_playerAnimFrame * 48, 0, 48, 48));
        }
        
        // Update coin animation
        int coinFrame = (_playerAnimFrame % 4);
        _tileSprites[COIN].setTextureRect(sf::IntRect(coinFrame * 16, 0, 16, 16));
        
        // Update electric hazard animation
        int electricFrame = (_playerAnimFrame % 2);
        _tileSprites[ELECTRIC].setTextureRect(sf::IntRect(electricFrame * 16, 0, 16, 48));
    }
    
    // Update background scrolling
    _backgroundScroll += 100.0f * deltaTime; // Adjust speed as needed
    if (_backgroundScroll >= _textures["background"].getSize().x) {
        _backgroundScroll = 0.0f;
    }
}

void GraphicsEngine::handleEvents()
{
    sf::Event event;
    while (_window.pollEvent(event)) {
        if (event.type == sf::Event::Closed) {
            _window.close();
        } 
        else if (event.type == sf::Event::KeyPressed) {
            if (event.key.code == sf::Keyboard::Space) {
                _jetpackActive = true;
            }
        } 
        else if (event.type == sf::Event::KeyReleased) {
            if (event.key.code == sf::Keyboard::Space) {
                _jetpackActive = false;
            }
        }
    }
}

void GraphicsEngine::render()
{
    _window.clear(sf::Color(44, 62, 80)); // Fond bleu foncé
    
    if (_mutex) {
        std::lock_guard<std::mutex> lock(*_mutex);
        
        // Dessiner le fond qui défile
        drawBackground();
        
        // Dessiner la carte
        drawMap();
        
        // Dessiner les joueurs
        drawPlayers();
        
        // Dessiner l'interface utilisateur
        drawUI();
    }
    
    _window.display();
}

void GraphicsEngine::drawBackground()
{
    // Draw a scrolling background (two copies side by side)
    float bgWidth = _textures["background"].getSize().x;
    float bgHeight = _textures["background"].getSize().y;
    
    // Scale background to fit window height
    float scaleFactor = static_cast<float>(_windowHeight) / bgHeight;
    _backgroundSprite.setScale(scaleFactor, scaleFactor);
    
    // Draw first background copy
    _backgroundSprite.setPosition(-_backgroundScroll, 0);
    _window.draw(_backgroundSprite);
    
    // Draw second background copy
    _backgroundSprite.setPosition(bgWidth * scaleFactor - _backgroundScroll, 0);
    _window.draw(_backgroundSprite);
}

void GraphicsEngine::drawMap()
{
    if (_map.empty()) {
        std::cerr << "Map is empty, nothing to draw" << std::endl;
        return;
    }
    
    for (size_t y = 0; y < _map.size(); ++y) {
        for (size_t x = 0; x < _map[y].size(); ++x) {
            TileType tile = _map[y][x];
            if (tile != EMPTY && tile != START_P1 && tile != START_P2) {
                sf::Sprite& sprite = _tileSprites[tile];
                sprite.setPosition(x * TILE_SIZE, y * TILE_SIZE);
                _window.draw(sprite);
            }
        }
    }
}

void GraphicsEngine::drawPlayers()
{
    for (int i = 0; i < _nbClients; ++i) {
        if (_playerAlive[i]) {
            // Position réelle = position * 100 (pour compenser la multiplication dans le serveur)
            float x = _playerPositions[i].first / 100.0f * TILE_SIZE;
            float y = _playerPositions[i].second / 100.0f * TILE_SIZE;
            
            if (i == _clientId) {
                _playerSprite.setPosition(x, y);
                _window.draw(_playerSprite);
                
                // Centrer la vue sur le joueur tout en laissant voir un peu devant
                _gameView.setCenter(x + _windowWidth/4, _windowHeight/2);
                _window.setView(_gameView);
            } else {
                _otherPlayerSprite.setPosition(x, y);
                _window.draw(_otherPlayerSprite);
            }
        }
    }
}

void GraphicsEngine::drawUI()
{
    // Dessiner les scores et états des joueurs
    sf::Text scoreText;
    scoreText.setFont(_font);
    scoreText.setCharacterSize(24);
    
    // Position fixe par rapport à la vue
    sf::View defaultView = _window.getDefaultView();
    _window.setView(defaultView);
    
    for (int i = 0; i < _nbClients; ++i) {
        std::string playerStatus;
        if (!_playerAlive[i]) {
            playerStatus = " (DEAD)";
        } else if (_gameState == PacketModule::ENDED) {
            playerStatus = " (FINISHED)";
        } else {
            playerStatus = "";
        }
        
        std::string text = "Player " + std::to_string(i + 1) + ": " + 
                           std::to_string(_playerScores[i]) + playerStatus;
        
        scoreText.setString(text);
        scoreText.setPosition(10, 10 + i * 30);
        
        if (i == _clientId) {
            scoreText.setFillColor(sf::Color::Yellow);
        } else {
            scoreText.setFillColor(sf::Color::White);
        }
        
        _window.draw(scoreText);
    }
    
    // Afficher des informations d'état du jeu
    sf::Text stateText;
    stateText.setFont(_font);
    stateText.setCharacterSize(24);
    stateText.setPosition(10, _windowHeight - 40);
    
    switch (_gameState) {
        case PacketModule::WAITING:
            stateText.setString("Waiting for players...");
            break;
        case PacketModule::PLAYING:
            stateText.setString("Game in progress - Space to activate jetpack");
            break;
        case PacketModule::ENDED:
            stateText.setString("Game over!");
            break;
        case PacketModule::WINNER:
            stateText.setString("You win!");
            break;
        case PacketModule::LOSER:
            stateText.setString("You lose!");
            break;
    }
    
    _window.draw(stateText);
    
    // Restaurer la vue du jeu
    _window.setView(_gameView);
}