#include "Graphics.hpp"
#include <iostream>

GraphicsEngine::GraphicsEngine()
    : _windowWidth(800), _windowHeight(600), _clientId(0), _nbClients(0), 
      _gameState(PacketModule::WAITING), _jetpackActive(false), _mutex(nullptr)
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
    
    return _window.isOpen();
}

bool GraphicsEngine::loadResources()
{
    // Charger les textures
    if (!_textures["player"].loadFromFile("assets/player.png")) {
        std::cerr << "Failed to load player texture" << std::endl;
        return false;
    }
    
    if (!_textures["otherPlayer"].loadFromFile("assets/player2.png")) {
        std::cerr << "Failed to load other player texture" << std::endl;
        return false;
    }
    
    if (!_textures["wall"].loadFromFile("assets/wall.png")) {
        std::cerr << "Failed to load wall texture" << std::endl;
        return false;
    }
    
    if (!_textures["coin"].loadFromFile("assets/coin.png")) {
        std::cerr << "Failed to load coin texture" << std::endl;
        return false;
    }
    
    if (!_textures["electric"].loadFromFile("assets/electric.png")) {
        std::cerr << "Failed to load electric texture" << std::endl;
        return false;
    }
    
    if (!_textures["finish"].loadFromFile("assets/finish.png")) {
        std::cerr << "Failed to load finish texture" << std::endl;
        return false;
    }
    
    // Charger la police
    if (!_font.loadFromFile("assets/font.ttf")) {
        std::cerr << "Failed to load font" << std::endl;
        return false;
    }
    
    // Configurer les sprites
    _playerSprite.setTexture(_textures["player"]);
    _playerSprite.setScale(1.0f, 1.0f);
    
    _otherPlayerSprite.setTexture(_textures["otherPlayer"]);
    _otherPlayerSprite.setScale(1.0f, 1.0f);
    
    _tileSprites[WALL].setTexture(_textures["wall"]);
    _tileSprites[COIN].setTexture(_textures["coin"]);
    _tileSprites[ELECTRIC].setTexture(_textures["electric"]);
    _tileSprites[FINISH].setTexture(_textures["finish"]);
    
    return true;
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
    } else {
        _map = map;
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
    while (_window.isOpen()) {
        handleEvents();
        render();
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
        
        // Dessiner la carte
        drawMap();
        
        // Dessiner les joueurs
        drawPlayers();
        
        // Dessiner l'interface utilisateur
        drawUI();
    }
    
    _window.display();
}

void GraphicsEngine::drawMap()
{
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
                
                // Centrer la vue sur le joueur
                _gameView.setCenter(x, y);
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
