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
    
    // Debug asset loading
    if (assetsLoaded) {
        std::cout << "Assets loaded successfully" << std::endl;
    } else {
        std::cout << "Failed to load assets" << std::endl;
    }
    
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
    
    // Setup animated sprites
    AnimatedSprite playerSprite, otherPlayerSprite;
    sf::Sprite backgroundSprite, endMarkerSprite;
    
    // Vectors to store game elements
    std::vector<std::pair<AnimatedSprite, bool>> animatedCoinSprites;  // sprite and collected status
    std::vector<AnimatedSprite> animatedElectricSprites;
    bool endMarkerExists = false;
    
    if (assetsLoaded) {
        // Check texture loading and sizes
        if (assets.hasTexture("player")) {
            sf::Vector2u playerSize = assets.getTexture("player").getSize();
            std::cout << "Player texture size: " << playerSize.x << "x" << playerSize.y << std::endl;
            
            // Player sprite sheet dimensions: 534 x 803 with 4 frames per row
            int playerFrameWidth = playerSize.x / 4;  // 534 / 4 = 133.5, round to 134
            int playerFrameHeight = 201;  // Assuming each animation is this height
            
            // Create player running animation
            Animation playerRunAnim;
            for (int i = 0; i < 4; i++) {
                playerRunAnim.addFrame(sf::IntRect(i * playerFrameWidth, 0, playerFrameWidth, playerFrameHeight));
            }
            playerRunAnim.setFrameTime(0.1f);
            
            // Create player jumping animation (assuming it's in the second row)
            Animation playerJumpAnim;
            for (int i = 0; i < 4; i++) {
                playerJumpAnim.addFrame(sf::IntRect(i * playerFrameWidth, playerFrameHeight, playerFrameWidth, playerFrameHeight));
            }
            playerJumpAnim.setFrameTime(0.1f);
            
            // Set up player sprite
            playerSprite.setTexture(assets.getTexture("player"));
            playerSprite.addAnimation("run", playerRunAnim);
            playerSprite.addAnimation("jump", playerJumpAnim);
            playerSprite.play("run");
            playerSprite.setScale(0.2f, 0.2f);  // Scale down to reasonable size
            
            // Set up other player sprite (same animations)
            otherPlayerSprite.setTexture(assets.getTexture("player"));
            otherPlayerSprite.addAnimation("run", playerRunAnim);
            otherPlayerSprite.addAnimation("jump", playerJumpAnim);
            otherPlayerSprite.play("run");
            otherPlayerSprite.setScale(0.2f, 0.2f);
            otherPlayerSprite.setColor(sf::Color(255, 100, 100));  // Tint to distinguish
        } else {
            std::cout << "Failed to load player texture" << std::endl;
        }
        
        // Set up background
        if (assets.hasTexture("background")) {
            backgroundSprite.setTexture(assets.getTexture("background"));
            std::cout << "Background texture loaded" << std::endl;
        } else {
            std::cout << "Failed to load background texture" << std::endl;
        }
    }
    
    // Setup sounds
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
        std::cout << "Font loaded" << std::endl;
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
    
    // Game clock for animations
    sf::Clock clock;
    
    // Game loop
    while (connected && window.isOpen()) {
        auto frameStart = std::chrono::high_resolution_clock::now();
        float deltaTime = clock.restart().asSeconds();
        
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
        
        // Handle jetpack sound
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
                
                // Play stop sound if available
                if (assets.hasSound("jetpack_stop")) {
                    sf::Sound stopSound;
                    stopSound.setBuffer(assets.getSound("jetpack_stop"));
                    stopSound.play();
                }
            }
        }
        
        // Update player animation
        if (assetsLoaded) {
            if (isJumping) {
                playerSprite.play("jump");
                otherPlayerSprite.play("jump");
            } else {
                playerSprite.play("run");
                otherPlayerSprite.play("run");
            }
            
            // Update animations
            playerSprite.update(deltaTime);
            otherPlayerSprite.update(deltaTime);
            
            // Update coin animations
            for (auto& [coinSprite, collected] : animatedCoinSprites) {
                if (!collected) {
                    coinSprite.update(deltaTime);
                }
            }
            
            // Update electric animations
            for (auto& electricSprite : animatedElectricSprites) {
                electricSprite.update(deltaTime);
            }
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
            
            std::cout << "Parsing map data, length: " << mapData.length() << std::endl;
            
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
            
            std::cout << "Map elements found: " << mapElements.size() << std::endl;
            
            // Setup game elements based on parsed map
            if (assetsLoaded) {
                // Check coin texture
                if (assets.hasTexture("coin")) {
                    sf::Vector2u coinSize = assets.getTexture("coin").getSize();
                    std::cout << "Coin texture size: " << coinSize.x << "x" << coinSize.y << std::endl;
                    
                    // Assuming coin sprite sheet has 6 frames horizontally
                    int coinFrameWidth = coinSize.x / 6;
                    int coinFrameHeight = coinSize.y;
                    
                    // Create coin animation
                    Animation coinAnim;
                    for (int i = 0; i < 6; i++) {
                        coinAnim.addFrame(sf::IntRect(i * coinFrameWidth, 0, coinFrameWidth, coinFrameHeight));
                    }
                    coinAnim.setFrameTime(0.1f);
                    
                    // Setup coins
                    for (const auto& element : mapElements) {
                        if (element.type == MapElement::COIN) {
                            AnimatedSprite sprite;
                            sprite.setTexture(assets.getTexture("coin"));
                            sprite.addAnimation("spin", coinAnim);
                            sprite.play("spin");
                            sprite.setPosition(element.position);
                            sprite.setScale(1.5f, 1.5f);  // Adjust scale as needed
                            
                            animatedCoinSprites.push_back(std::make_pair(sprite, false));
                        }
                    }
                }
                
                // Check electric texture
                if (assets.hasTexture("electric")) {
                    sf::Vector2u electricSize = assets.getTexture("electric").getSize();
                    std::cout << "Electric texture size: " << electricSize.x << "x" << electricSize.y << std::endl;
                    
                    // Assuming electric sprite sheet has 4 frames horizontally
                    int electricFrameWidth = electricSize.x / 4;
                    int electricFrameHeight = electricSize.y;
                    
                    // Create electric animation
                    Animation electricAnim;
                    for (int i = 0; i < 4; i++) {
                        electricAnim.addFrame(sf::IntRect(i * electricFrameWidth, 0, electricFrameWidth, electricFrameHeight));
                    }
                    electricAnim.setFrameTime(0.15f);
                    
                    // Setup electric obstacles
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
                
                // Setup end marker
                for (const auto& element : mapElements) {
                    if (element.type == MapElement::END_MARKER) {
                        // Use player texture as placeholder
                        endMarkerSprite.setTexture(assets.getTexture("player"));
                        endMarkerSprite.setColor(sf::Color::Green);
                        endMarkerSprite.setPosition(element.position);
                        endMarkerExists = true;
                        break;
                    }
                }
            } else {
                // Setup for fallback rendering with colored rectangles
                // (Code for fallback rectangles is handled in the render section)
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
            playerSprite.setPosition(100, playerPosition.y);
            playerBounds = playerSprite.getGlobalBounds();
        } else {
            playerRect.setPosition(100, playerPosition.y);
            playerBounds = playerRect.getGlobalBounds();
        }
        
        // Check coin collisions
        for (auto& [coinSprite, collected] : animatedCoinSprites) {
            if (collected) continue;
            
            // Adjust position for scrolling
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
        
        // Check electric square collisions
        for (auto& electricSprite : animatedElectricSprites) {
            // Adjust position for scrolling
            electricSprite.setPosition(electricSprite.getPosition().x - mapOffset, electricSprite.getPosition().y);
            sf::FloatRect electricBounds = electricSprite.getGlobalBounds();
            
            if (playerBounds.intersects(electricBounds)) {
                // Player died, send notification to server through packet
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
        
        // Check end marker collision
        if (endMarkerExists) {
            endMarkerSprite.setPosition(endMarkerSprite.getPosition().x - mapOffset, endMarkerSprite.getPosition().y);
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
        if (assetsLoaded && assets.hasTexture("background")) {
            // Parallax scrolling effect
            float bgOffset = mapOffset * 0.5f;  // Background moves slower than foreground
            backgroundSprite.setPosition(-bgOffset, 0);
            window.draw(backgroundSprite);
        } else {
            window.draw(backgroundRect);
        }
        
        // Draw coins
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
        
        // Draw electric squares
        for (const auto& electricSprite : animatedElectricSprites) {
            if (assetsLoaded && assets.hasTexture("electric")) {
                window.draw(electricSprite);
            } else {
                sf::RectangleShape adjustedElectric = electricRect;
                adjustedElectric.setPosition(electricSprite.getPosition());
                window.draw(adjustedElectric);
            }
        }
        
        // Draw end marker
        if (endMarkerExists) {
            if (assetsLoaded) {
                window.draw(endMarkerSprite);
            } else {
                sf::RectangleShape adjustedEndMarker = endMarkerRect;
                adjustedEndMarker.setPosition(endMarkerSprite.getPosition());
                window.draw(adjustedEndMarker);
            }
        }
        
        // Draw player
        if (assetsLoaded && assets.hasTexture("player")) {
            // Main player is always at x=100
            window.draw(playerSprite);
            
            // Draw other player
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