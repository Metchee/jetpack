#include "../shared_include/Client.hpp"
#include "../shared_include/AssetManager.hpp"
#include "../shared_include/Animation.hpp"
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <iostream>
#include <string>
#include <mutex>
#include <chrono>
#include <thread>
#include <memory>
#include <sstream>

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const float PLAYER_SPEED = 5.0f;
const float GRAVITY = 0.5f;
const float JUMP_FORCE = -8.0f;
const float PLAYER_SIZE = 40.0f;
const float HORIZONTAL_SCROLL_SPEED = 3.0f;

void ClientModule::Client::gameThread()
{
    sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "Jetpack Game");
    window.setFramerateLimit(60);
    
    AssetManager assets;
    bool assetsLoaded = assets.loadDefaultAssets();
    
    if (assetsLoaded) {
        std::cout << "Assets loaded successfully" << std::endl;
    } else {
        std::cout << "Failed to load assets" << std::endl;
    }
    
    sf::RectangleShape playerRect(sf::Vector2f(PLAYER_SIZE, PLAYER_SIZE));
    playerRect.setFillColor(sf::Color::Blue);
    
    sf::RectangleShape coinRect(sf::Vector2f(PLAYER_SIZE * 0.75f, PLAYER_SIZE * 0.75f));
    coinRect.setFillColor(sf::Color::Yellow);
    
    sf::RectangleShape electricRect(sf::Vector2f(PLAYER_SIZE, PLAYER_SIZE));
    electricRect.setFillColor(sf::Color::Red);
    
    sf::RectangleShape endMarkerRect(sf::Vector2f(PLAYER_SIZE, WINDOW_HEIGHT));
    endMarkerRect.setFillColor(sf::Color::Green);
    
    sf::RectangleShape backgroundRect(sf::Vector2f(WINDOW_WIDTH, WINDOW_HEIGHT));
    backgroundRect.setFillColor(sf::Color(50, 50, 150));
    
    AnimatedSprite playerSprite, otherPlayerSprite;
    sf::Sprite backgroundSprite, endMarkerSprite;
    
    std::vector<std::pair<AnimatedSprite, bool>> animatedCoinSprites; 
    std::vector<AnimatedSprite> animatedElectricSprites;
    bool endMarkerExists = false;
    
    if (assetsLoaded) {
        if (assets.hasTexture("player")) {
            sf::Vector2u playerSize = assets.getTexture("player").getSize();
            std::cout << "Player texture size: " << playerSize.x << "x" << playerSize.y << std::endl;
            
            int playerFrameWidth = playerSize.x / 4;
            int playerFrameHeight = 133;
            
            Animation playerRunAnim;
            for (int i = 0; i < 4; i++) {
                playerRunAnim.addFrame(sf::IntRect(i * playerFrameWidth, 0, playerFrameWidth, playerFrameHeight));
            }
            playerRunAnim.setFrameTime(0.1f);
            
            Animation playerJumpAnim;
            for (int i = 0; i < 4; i++) {
                playerJumpAnim.addFrame(sf::IntRect(i * playerFrameWidth, playerFrameHeight, playerFrameWidth, playerFrameHeight));
            }
            playerJumpAnim.setFrameTime(0.1f);
            
            playerSprite.setTexture(assets.getTexture("player"));
            playerSprite.addAnimation("run", playerRunAnim);
            playerSprite.addAnimation("jump", playerJumpAnim);
            playerSprite.play("run");
            playerSprite.setScale(0.5f, 0.5f);
            
            otherPlayerSprite.setTexture(assets.getTexture("player"));
            otherPlayerSprite.addAnimation("run", playerRunAnim);
            otherPlayerSprite.addAnimation("jump", playerJumpAnim);
            otherPlayerSprite.play("run");
            otherPlayerSprite.setScale(0.5f, 0.5f);
            otherPlayerSprite.setColor(sf::Color(255, 100, 100));
        } else {
            std::cout << "Failed to load player texture" << std::endl;
        }
        
        if (assets.hasTexture("background")) {
            backgroundSprite.setTexture(assets.getTexture("background"));
            
            sf::Vector2u backgroundSize = assets.getTexture("background").getSize();
            std::cout << "Background texture size: " << backgroundSize.x << "x" << backgroundSize.y << std::endl;
            backgroundSprite.setScale(2.0f, 2.0f);
            std::cout << "Background texture loaded" << std::endl;
        } else {
            std::cout << "Failed to load background texture" << std::endl;
        }
    }
    
    sf::Sound jumpSound, coinSound, deathSound, jetpackSound;
    bool jetpackSoundPlaying = false;
    
    if (assetsLoaded) {
        if (assets.hasSound("jump")) {
            jumpSound.setBuffer(assets.getSound("jump"));
            std::cout << "Jump sound loaded" << std::endl;
        }
        
        if (assets.hasSound("coin")) {
            coinSound.setBuffer(assets.getSound("coin"));
            std::cout << "Coin sound loaded" << std::endl;
        }
        
        if (assets.hasSound("death")) {
            deathSound.setBuffer(assets.getSound("death"));
            std::cout << "Death sound loaded" << std::endl;
        }
        
        if (assets.hasSound("jetpack")) {
            jetpackSound.setBuffer(assets.getSound("jetpack"));
            jetpackSound.setLoop(true);
            std::cout << "Jetpack sound loaded" << std::endl;
        }
    }
    
    bool isJumping = false;
    bool wasJumping = false;
    float verticalVelocity = 0.0f;
    sf::Vector2f playerPosition(100, WINDOW_HEIGHT / 2);
    sf::Vector2f otherPlayerPosition(100, WINDOW_HEIGHT / 2 + 50);
    int myScore = 0;
    int otherScore = 0;
    float mapOffset = 0.0f;
    
    sf::Font font;
    if (assetsLoaded && assets.hasFont("main")) {
        font = assets.getFont("main");
        std::cout << "Font loaded" << std::endl;
    } else {
        if (!font.loadFromFile("assets/jetpack_font.ttf")) {
            std::cerr << "Failed to load font" << std::endl;
        }
    }
    sf::Text scoreText;
    scoreText.setFont(font);
    scoreText.setCharacterSize(24);
    scoreText.setFillColor(sf::Color::White);
    scoreText.setPosition(10, 10);
    
    struct MapElement {
        enum Type { EMPTY, COIN, ELECTRIC, END_MARKER };
        Type type;
        sf::Vector2f position;
    };
    std::vector<MapElement> mapElements;
    bool mapParsed = false;
    sf::Clock clock;
    while (connected && window.isOpen()) {
        auto frameStart = std::chrono::high_resolution_clock::now();
        float deltaTime = clock.restart().asSeconds();
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
                stop();
            }
        }
        wasJumping = isJumping;
        isJumping = sf::Keyboard::isKeyPressed(sf::Keyboard::Space);
        if (isJumping && !wasJumping && assetsLoaded) {
            jumpSound.play();
            
            if (!jetpackSoundPlaying && assets.hasSound("jetpack")) {
                jetpackSound.play();
                jetpackSoundPlaying = true;
            }
        } else if (!isJumping && wasJumping && assetsLoaded) {
            if (jetpackSoundPlaying) {
                jetpackSound.stop();
                jetpackSoundPlaying = false;
                if (assets.hasSound("jetpack_stop")) {
                    sf::Sound stopSound;
                    stopSound.setBuffer(assets.getSound("jetpack_stop"));
                    stopSound.play();
                }
            }
        }
        if (assetsLoaded) {
            if (isJumping) {
                playerSprite.play("jump");
                otherPlayerSprite.play("jump");
            } else {
                playerSprite.play("run");
                otherPlayerSprite.play("run");
            }
            playerSprite.update(deltaTime);
            otherPlayerSprite.update(deltaTime);
            for (auto& [coinSprite, collected] : animatedCoinSprites) {
                if (!collected) {
                    coinSprite.update(deltaTime);
                }
            }
            for (auto& electricSprite : animatedElectricSprites) {
                electricSprite.update(deltaTime);
            }
        }
        PacketModule current_state;
        {
            std::lock_guard<std::mutex> lock(_packetMutex);
            current_state = packet;
        }
        int playerId = current_state.getClientId();
        auto gameState = current_state.getstate();
        if (!mapParsed && std::strlen(current_state.getPacket().map) > 0) {
            std::string mapData = current_state.getPacket().map;
            mapParsed = true;
            std::istringstream stream(mapData);
            std::string line;
            int y = 0;
            while (std::getline(stream, line)) {
                for (size_t x = 0; x < line.length(); x++) {
                    if (line[x] == 'C') {
                        MapElement element;
                        element.type = MapElement::COIN;
                        element.position = sf::Vector2f(x * 40 + 200, y * 40 + 100);
                        mapElements.push_back(element);
                    }
                    else if (line[x] == 'E') {
                        MapElement element;
                        element.type = MapElement::ELECTRIC;
                        element.position = sf::Vector2f(x * 40 + 200, y * 40 + 100);
                        mapElements.push_back(element);
                    }
                    else if (line[x] == 'F') {
                        MapElement element;
                        element.type = MapElement::END_MARKER;
                        element.position = sf::Vector2f(x * 40 + 200, y * 40 + 100);
                        mapElements.push_back(element);
                    }
                }
                y++;
            }
            if (assetsLoaded) {
                if (assets.hasTexture("coin")) {
                    sf::Vector2u coinSize = assets.getTexture("coin").getSize();
                    int coinFrameWidth = coinSize.x / 6;
                    int coinFrameHeight = coinSize.y;
                    Animation coinAnim;
                    for (int i = 0; i < 6; i++) {
                        coinAnim.addFrame(sf::IntRect(i * coinFrameWidth, 0, coinFrameWidth, coinFrameHeight));
                    }
                    coinAnim.setFrameTime(0.1f);
                    for (const auto& element : mapElements) {
                        if (element.type == MapElement::COIN) {
                            AnimatedSprite sprite;
                            sprite.setTexture(assets.getTexture("coin"));
                            sprite.addAnimation("spin", coinAnim);
                            sprite.play("spin");
                            sprite.setPosition(element.position);
                            sprite.setScale(1.5f, 1.5f);
                            animatedCoinSprites.push_back(std::make_pair(sprite, false));
                        }
                    }
                }
                if (assets.hasTexture("electric")) {
                    sf::Vector2u electricSize = assets.getTexture("electric").getSize();
                    std::cout << "Electric texture size: " << electricSize.x << "x" << electricSize.y << std::endl;
                    int electricFrameWidth = electricSize.x / 4;
                    int electricFrameHeight = electricSize.y;
                    Animation electricAnim;
                    for (int i = 0; i < 4; i++) {
                        electricAnim.addFrame(sf::IntRect(i * electricFrameWidth, 0, electricFrameWidth, electricFrameHeight));
                    }
                    electricAnim.setFrameTime(0.15f);
                    for (const auto& element : mapElements) {
                        if (element.type == MapElement::ELECTRIC) {
                            AnimatedSprite sprite;
                            sprite.setTexture(assets.getTexture("electric"));
                            sprite.addAnimation("zap", electricAnim);
                            sprite.play("zap");
                            sprite.setPosition(element.position);
                            
                            animatedElectricSprites.push_back(sprite);
                        }
                    }
                }
                for (const auto& element : mapElements) {
                    if (element.type == MapElement::END_MARKER) {
                        endMarkerSprite.setTexture(assets.getTexture("player"));
                        endMarkerSprite.setColor(sf::Color::Green);
                        endMarkerSprite.setPosition(element.position);
                        endMarkerExists = true;
                        break;
                    }
                }
            }
        }
        if (playerId < current_state.getNbClient()) {
            auto myPos = current_state.getPosition();
            playerPosition.x = myPos.first;
            playerPosition.y = myPos.second;
            int otherPlayerId = (playerId == 0) ? 1 : 0;
            if (otherPlayerId < current_state.getNbClient()) {
                auto otherPos = current_state.getPacket().playerPosition[otherPlayerId];
                otherPlayerPosition.x = otherPos.first;
                otherPlayerPosition.y = otherPos.second;
            }
        }
        if (isJumping) {
            verticalVelocity = JUMP_FORCE;
        } else {
            verticalVelocity += GRAVITY;
        }
        
        playerPosition.y += verticalVelocity;
        playerPosition.x += HORIZONTAL_SCROLL_SPEED;
        if (playerPosition.y < 0) {
            playerPosition.y = 0;
            verticalVelocity = 0;
        } else if (playerPosition.y > WINDOW_HEIGHT - PLAYER_SIZE) {
            playerPosition.y = WINDOW_HEIGHT - PLAYER_SIZE;
            verticalVelocity = 0;
        }
        mapOffset = playerPosition.x - 100.0f;
        {
            std::lock_guard<std::mutex> lock(_packetMutex);
            packet.getPacket().playerPosition[playerId] = std::make_pair(
                static_cast<int>(playerPosition.x), 
                static_cast<int>(playerPosition.y)
            );
        }
        sf::FloatRect playerBounds;
        if (assetsLoaded) {
            playerSprite.setPosition(100, playerPosition.y);
            playerBounds = playerSprite.getGlobalBounds();
        } else {
            playerRect.setPosition(100, playerPosition.y);
            playerBounds = playerRect.getGlobalBounds();
        }
        for (auto& [coinSprite, collected] : animatedCoinSprites) {
            if (collected) continue;
            coinSprite.setPosition(coinSprite.getPosition().x - mapOffset, coinSprite.getPosition().y);
            sf::FloatRect coinBounds = coinSprite.getGlobalBounds();
            
            if (playerBounds.intersects(coinBounds)) {
                collected = true;
                myScore++;
                
                if (assetsLoaded && assets.hasSound("coin")) {
                    coinSound.play();
                }
            }
        }
        for (auto& electricSprite : animatedElectricSprites) {
            electricSprite.setPosition(electricSprite.getPosition().x - mapOffset, electricSprite.getPosition().y);
            sf::FloatRect electricBounds = electricSprite.getGlobalBounds();
            
            if (playerBounds.intersects(electricBounds)) {
                std::lock_guard<std::mutex> lock(_packetMutex);
                packet.getPacket().playerState[playerId] = PacketModule::ENDED;
                
                if (assetsLoaded && assets.hasSound("death")) {
                    deathSound.play();
                    if (jetpackSoundPlaying) {
                        jetpackSound.stop();
                        jetpackSoundPlaying = false;
                    }
                }
            }
        }
        if (endMarkerExists) {
            endMarkerSprite.setPosition(endMarkerSprite.getPosition().x - mapOffset, endMarkerSprite.getPosition().y);
            sf::FloatRect endMarkerBounds = endMarkerSprite.getGlobalBounds();
            
            if (playerBounds.intersects(endMarkerBounds)) {
                std::lock_guard<std::mutex> lock(_packetMutex);
                packet.getPacket().playerState[playerId] = PacketModule::ENDED;
            }
        }
        scoreText.setString("Your Score: " + std::to_string(myScore) + 
                          "\nOther Player: " + std::to_string(otherScore));
        window.clear();
        if (assetsLoaded && assets.hasTexture("background")) {
            float bgOffset = mapOffset * 0.5f;
            backgroundSprite.setPosition(-bgOffset, 0);
            window.draw(backgroundSprite);
        } else {
            window.draw(backgroundRect);
        }
        for (const auto& [coinSprite, collected] : animatedCoinSprites) {
            if (collected) continue;
            
            if (assetsLoaded && assets.hasTexture("coin")) {
                window.draw(coinSprite);
            } else {
                sf::RectangleShape adjustedCoin = coinRect;
                adjustedCoin.setPosition(coinSprite.getPosition());
                window.draw(adjustedCoin);
            }
        }
        for (const auto& electricSprite : animatedElectricSprites) {
            if (assetsLoaded && assets.hasTexture("electric")) {
                window.draw(electricSprite);
            } else {
                sf::RectangleShape adjustedElectric = electricRect;
                adjustedElectric.setPosition(electricSprite.getPosition());
                window.draw(adjustedElectric);
            }
        }
        if (endMarkerExists) {
            if (assetsLoaded) {
                window.draw(endMarkerSprite);
            } else {
                sf::RectangleShape adjustedEndMarker = endMarkerRect;
                adjustedEndMarker.setPosition(endMarkerSprite.getPosition());
                window.draw(adjustedEndMarker);
            }
        }
        if (assetsLoaded && assets.hasTexture("player")) {
            window.draw(playerSprite);
            otherPlayerSprite.setPosition(otherPlayerPosition.x - mapOffset, otherPlayerPosition.y);
            window.draw(otherPlayerSprite);
        } else {
            playerRect.setPosition(100, playerPosition.y);
            window.draw(playerRect);
            sf::RectangleShape otherPlayerRect = playerRect;
            otherPlayerRect.setFillColor(sf::Color::Red);
            otherPlayerRect.setPosition(otherPlayerPosition.x - mapOffset, otherPlayerPosition.y);
            window.draw(otherPlayerRect);
        }
        window.draw(scoreText);
        if (gameState == PacketModule::ENDED) {
            sf::Text gameOverText;
            gameOverText.setFont(font);
            gameOverText.setCharacterSize(36);
            gameOverText.setFillColor(sf::Color::White);
            gameOverText.setString("Game Over!");
            
            sf::FloatRect textBounds = gameOverText.getLocalBounds();
            gameOverText.setOrigin(textBounds.width / 2, textBounds.height / 2);
            gameOverText.setPosition(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
            
            window.draw(gameOverText);
        } else if (gameState == PacketModule::WAITING) {
            sf::Text waitingText;
            waitingText.setFont(font);
            waitingText.setCharacterSize(36);
            waitingText.setFillColor(sf::Color::White);
            waitingText.setString("Waiting for players...");
            sf::FloatRect textBounds = waitingText.getLocalBounds();
            waitingText.setOrigin(textBounds.width / 2, textBounds.height / 2);
            waitingText.setPosition(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
            
            window.draw(waitingText);
        }
        
        window.display();
        auto frameEnd = std::chrono::high_resolution_clock::now();
        auto frameDuration = std::chrono::duration_cast<std::chrono::milliseconds>(frameEnd - frameStart).count();
        int sleepTime = std::max(0, 16 - static_cast<int>(frameDuration));
        
        if (sleepTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
        }
    }
}