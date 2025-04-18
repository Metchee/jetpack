#include "../shared_include/Client.hpp"
#include "../shared_include/AssetManager.hpp"
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <iostream>
#include <string>
#include <mutex>
#include <chrono>
#include <thread>
#include <memory>
#include <sstream>

// Game constants
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const float PLAYER_SPEED = 5.0f;
const float GRAVITY = 0.5f;
const float JUMP_FORCE = -8.0f;
const float PLAYER_SIZE = 40.0f;
const float HORIZONTAL_SCROLL_SPEED = 3.0f;

void ClientModule::Client::gameThread()
{
    // Setup SFML window
    sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "Jetpack Game");
    window.setFramerateLimit(60);
    
    // Initialize asset manager
    AssetManager assets;
    bool assetsLoaded = assets.loadDefaultAssets();
    
    // Setup fallback shapes if assets couldn't be loaded
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
    
    // Setup sprites
    sf::Sprite playerSprite, otherPlayerSprite, backgroundSprite;
    
    if (assetsLoaded) {
        playerSprite.setTexture(assets.getTexture("player"));
        otherPlayerSprite.setTexture(assets.getTexture("player"));
        otherPlayerSprite.setColor(sf::Color(255, 100, 100)); // Tint to distinguish
        backgroundSprite.setTexture(assets.getTexture("background"));
    }
    
    // Setup sounds
    sf::Sound jumpSound, coinSound, deathSound;
    
    if (assetsLoaded) {
        jumpSound.setBuffer(assets.getSound("jump"));
        coinSound.setBuffer(assets.getSound("coin"));
        deathSound.setBuffer(assets.getSound("death"));
    }
    
    // Game state variables
    bool isJumping = false;
    bool wasJumping = false;
    float verticalVelocity = 0.0f;
    sf::Vector2f playerPosition(100, WINDOW_HEIGHT / 2);
    sf::Vector2f otherPlayerPosition(100, WINDOW_HEIGHT / 2 + 50);
    int myScore = 0;
    int otherScore = 0;
    float mapOffset = 0.0f;  // For horizontal scrolling
    
    // Setup text for displaying scores and messages
    sf::Font font;
    if (assetsLoaded && assets.hasFont("main")) {
        font = assets.getFont("main");
    } else {
        // Try to load a fallback font or use default
        if (!font.loadFromFile("assets/jetpack_font.ttf")) {
            std::cerr << "Failed to load font" << std::endl;
        }
    }
    
    sf::Text scoreText;
    scoreText.setFont(font);
    scoreText.setCharacterSize(24);
    scoreText.setFillColor(sf::Color::White);
    scoreText.setPosition(10, 10);
    
    // Define map element structure
    struct MapElement {
        enum Type { EMPTY, COIN, ELECTRIC, END_MARKER };
        Type type;
        sf::Vector2f position;
    };
    
    // Map data structures
    std::vector<MapElement> mapElements;
    bool mapParsed = false;
    
    // Vectors to store game elements
    std::vector<std::pair<sf::Sprite, bool>> coinSprites;  // sprite and collected status
    std::vector<sf::Sprite> electricSprites;
    sf::Sprite endMarkerSprite;
    bool endMarkerExists = false;
    
    // Game loop
    while (connected && window.isOpen()) {
        auto frameStart = std::chrono::high_resolution_clock::now();
        
        // Handle window events
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
                stop(); // Stop the client
            }
        }
        
        // Handle player input
        wasJumping = isJumping;
        isJumping = sf::Keyboard::isKeyPressed(sf::Keyboard::Space);
        
        // Play jump sound when starting to jump
        if (isJumping && !wasJumping && assetsLoaded) {
            jumpSound.play();
        }
        
        // Get current game state from network thread
        PacketModule current_state;
        {
            std::lock_guard<std::mutex> lock(_packetMutex);
            current_state = packet;
        }
        
        // Get player ID and current state
        int playerId = current_state.getClientId();
        auto gameState = current_state.getstate();
        
        // Parse map data if received and not yet processed
        if (!mapParsed && std::strlen(current_state.getPacket().map) > 0) {
            std::string mapData = current_state.getPacket().map;
            mapParsed = true;
            
            // Parse map string manually
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
            
            // Setup game elements based on parsed map
            if (assetsLoaded) {
                // Setup coins
                for (const auto& element : mapElements) {
                    if (element.type == MapElement::COIN) {
                        sf::Sprite sprite;
                        sprite.setTexture(assets.getTexture("coin"));
                        sprite.setPosition(element.position);
                        coinSprites.push_back(std::make_pair(sprite, false));  // not collected
                    }
                    else if (element.type == MapElement::ELECTRIC) {
                        sf::Sprite sprite;
                        sprite.setTexture(assets.getTexture("electric"));
                        sprite.setPosition(element.position);
                        electricSprites.push_back(sprite);
                    }
                    else if (element.type == MapElement::END_MARKER) {
                        endMarkerSprite.setTexture(assets.getTexture("player"));  // Use player texture as placeholder
                        endMarkerSprite.setColor(sf::Color::Green);
                        endMarkerSprite.setPosition(element.position);
                        endMarkerExists = true;
                    }
                }
            } else {
                // Setup for fallback rendering with colored rectangles
                for (const auto& element : mapElements) {
                    if (element.type == MapElement::COIN) {
                        sf::Sprite sprite;
                        sprite.setPosition(element.position);
                        coinSprites.push_back(std::make_pair(sprite, false));
                    }
                    else if (element.type == MapElement::ELECTRIC) {
                        sf::Sprite sprite;
                        sprite.setPosition(element.position);
                        electricSprites.push_back(sprite);
                    }
                    else if (element.type == MapElement::END_MARKER) {
                        endMarkerSprite.setPosition(element.position);
                        endMarkerExists = true;
                    }
                }
            }
        }
        
        // Update player positions from packet
        if (playerId < current_state.getNbClient()) {
            // Get my position
            auto myPos = current_state.getPosition();
            playerPosition.x = myPos.first;
            playerPosition.y = myPos.second;
            
            // Get other player position
            // This assumes only 2 players; would need to be modified for more
            int otherPlayerId = (playerId == 0) ? 1 : 0;
            if (otherPlayerId < current_state.getNbClient()) {
                auto otherPos = current_state.getPacket().playerPosition[otherPlayerId];
                otherPlayerPosition.x = otherPos.first;
                otherPlayerPosition.y = otherPos.second;
            }
        }
        
        // Apply physics (only for local prediction, server has final say)
        if (isJumping) {
            verticalVelocity = JUMP_FORCE;
        } else {
            verticalVelocity += GRAVITY;
        }
        
        playerPosition.y += verticalVelocity;
        playerPosition.x += HORIZONTAL_SCROLL_SPEED;  // Constant horizontal movement
        
        // Constrain player to screen (map limits)
        if (playerPosition.y < 0) {
            playerPosition.y = 0;
            verticalVelocity = 0;
        } else if (playerPosition.y > WINDOW_HEIGHT - PLAYER_SIZE) {
            playerPosition.y = WINDOW_HEIGHT - PLAYER_SIZE;
            verticalVelocity = 0;
        }
        
        // Update map offset for scrolling
        mapOffset = playerPosition.x - 100.0f;  // Keep player at x=100
        
        // Update player position in packet for network thread
        {
            std::lock_guard<std::mutex> lock(_packetMutex);
            packet.getPacket().playerPosition[playerId] = std::make_pair(
                static_cast<int>(playerPosition.x), 
                static_cast<int>(playerPosition.y)
            );
        }
        
        // Check collisions
        sf::FloatRect playerBounds;
        if (assetsLoaded) {
            playerSprite.setPosition(playerPosition);
            playerBounds = playerSprite.getGlobalBounds();
        } else {
            playerRect.setPosition(playerPosition);
            playerBounds = playerRect.getGlobalBounds();
        }
        
        // Check coin collisions
        for (auto& [coinSprite, collected] : coinSprites) {
            if (collected) continue;
            
            sf::FloatRect coinBounds = coinSprite.getGlobalBounds();
            if (playerBounds.intersects(coinBounds)) {
                collected = true;
                myScore++;
                
                if (assetsLoaded) {
                    coinSound.play();
                }
            }
        }
        
        // Check electric square collisions
        for (const auto& electricSprite : electricSprites) {
            sf::FloatRect electricBounds = electricSprite.getGlobalBounds();
            if (playerBounds.intersects(electricBounds)) {
                // Player died, send notification to server through packet
                std::lock_guard<std::mutex> lock(_packetMutex);
                packet.getPacket().playerState[playerId] = PacketModule::ENDED;
                
                if (assetsLoaded) {
                    deathSound.play();
                }
            }
        }
        
        // Check end marker collision
        if (endMarkerExists) {
            sf::FloatRect endMarkerBounds = endMarkerSprite.getGlobalBounds();
            if (playerBounds.intersects(endMarkerBounds)) {
                // Player reached the end, send notification to server
                std::lock_guard<std::mutex> lock(_packetMutex);
                packet.getPacket().playerState[playerId] = PacketModule::ENDED;
            }
        }
        
        // Update score display
        scoreText.setString("Your Score: " + std::to_string(myScore) + 
                          "\nOther Player: " + std::to_string(otherScore));
        
        // Render the game
        window.clear();
        
        // Draw background
        if (assetsLoaded) {
            // Parallax scrolling effect
            float bgOffset = mapOffset * 0.5f;  // Background moves slower than foreground
            backgroundSprite.setPosition(-bgOffset, 0);
            window.draw(backgroundSprite);
        } else {
            window.draw(backgroundRect);
        }
        
        // Draw coins
        for (const auto& [coinSprite, collected] : coinSprites) {
            if (collected) continue;
            
            if (assetsLoaded) {
                sf::Sprite adjustedSprite = coinSprite;
                adjustedSprite.setPosition(coinSprite.getPosition().x - mapOffset, coinSprite.getPosition().y);
                window.draw(adjustedSprite);
            } else {
                sf::RectangleShape adjustedCoin = coinRect;
                adjustedCoin.setPosition(coinSprite.getPosition().x - mapOffset, coinSprite.getPosition().y);
                window.draw(adjustedCoin);
            }
        }
        
        // Draw electric squares
        for (const auto& electricSprite : electricSprites) {
            if (assetsLoaded) {
                sf::Sprite adjustedSprite = electricSprite;
                adjustedSprite.setPosition(electricSprite.getPosition().x - mapOffset, electricSprite.getPosition().y);
                window.draw(adjustedSprite);
            } else {
                sf::RectangleShape adjustedElectric = electricRect;
                adjustedElectric.setPosition(electricSprite.getPosition().x - mapOffset, electricSprite.getPosition().y);
                window.draw(adjustedElectric);
            }
        }
        
        // Draw end marker
        if (endMarkerExists) {
            if (assetsLoaded) {
                sf::Sprite adjustedSprite = endMarkerSprite;
                adjustedSprite.setPosition(endMarkerSprite.getPosition().x - mapOffset, endMarkerSprite.getPosition().y);
                window.draw(adjustedSprite);
            } else {
                sf::RectangleShape adjustedEndMarker = endMarkerRect;
                adjustedEndMarker.setPosition(endMarkerSprite.getPosition().x - mapOffset, endMarkerSprite.getPosition().y);
                window.draw(adjustedEndMarker);
            }
        }
        
        // Draw player
        if (assetsLoaded) {
            // Always keep player at x=100 on screen (scroll the map instead)
            playerSprite.setPosition(100, playerPosition.y);
            window.draw(playerSprite);
            
            // Draw other player relative to the map offset
            otherPlayerSprite.setPosition(otherPlayerPosition.x - mapOffset, otherPlayerPosition.y);
            window.draw(otherPlayerSprite);
        } else {
            playerRect.setPosition(100, playerPosition.y);
            window.draw(playerRect);
            
            // Draw other player
            sf::RectangleShape otherPlayerRect = playerRect;
            otherPlayerRect.setFillColor(sf::Color::Red);
            otherPlayerRect.setPosition(otherPlayerPosition.x - mapOffset, otherPlayerPosition.y);
            window.draw(otherPlayerRect);
        }
        
        // Draw score
        window.draw(scoreText);
        
        // Draw game state messages
        if (gameState == PacketModule::ENDED) {
            sf::Text gameOverText;
            gameOverText.setFont(font);
            gameOverText.setCharacterSize(36);
            gameOverText.setFillColor(sf::Color::White);
            gameOverText.setString("Game Over!");
            
            // Center the text
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
            
            // Center the text
            sf::FloatRect textBounds = waitingText.getLocalBounds();
            waitingText.setOrigin(textBounds.width / 2, textBounds.height / 2);
            waitingText.setPosition(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
            
            window.draw(waitingText);
        }
        
        window.display();
        
        // Frame rate control to maintain consistent gameplay speed
        auto frameEnd = std::chrono::high_resolution_clock::now();
        auto frameDuration = std::chrono::duration_cast<std::chrono::milliseconds>(frameEnd - frameStart).count();
        int sleepTime = std::max(0, 16 - static_cast<int>(frameDuration));  // Target ~60 FPS
        
        if (sleepTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
        }
    }
}