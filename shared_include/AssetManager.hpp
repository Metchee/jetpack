#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <string>
#include <map>
#include <memory>
#include <iostream>

// A simple asset manager to handle loading and retrieving textures, fonts, and sounds
class AssetManager {
public:
    AssetManager() = default;
    ~AssetManager() = default;
    
    // Load a texture from file
    bool loadTexture(const std::string& name, const std::string& filename) {
        sf::Texture texture;
        if (!texture.loadFromFile(filename)) {
            std::cerr << "Failed to load texture: " << filename << std::endl;
            return false;
        }
        
        textures[name] = texture;
        return true;
    }
    
    // Get a texture by name
    sf::Texture& getTexture(const std::string& name) {
        return textures[name];
    }
    
    // Check if a texture exists
    bool hasTexture(const std::string& name) const {
        return textures.find(name) != textures.end();
    }
    
    // Load a font from file
    bool loadFont(const std::string& name, const std::string& filename) {
        sf::Font font;
        if (!font.loadFromFile(filename)) {
            std::cerr << "Failed to load font: " << filename << std::endl;
            return false;
        }
        
        fonts[name] = font;
        return true;
    }
    
    // Get a font by name
    sf::Font& getFont(const std::string& name) {
        return fonts[name];
    }
    
    // Check if a font exists
    bool hasFont(const std::string& name) const {
        return fonts.find(name) != fonts.end();
    }
    
    // Load a sound from file
    bool loadSound(const std::string& name, const std::string& filename) {
        sf::SoundBuffer buffer;
        if (!buffer.loadFromFile(filename)) {
            std::cerr << "Failed to load sound: " << filename << std::endl;
            return false;
        }
        
        sounds[name] = buffer;
        return true;
    }
    
    // Get a sound by name
    sf::SoundBuffer& getSound(const std::string& name) {
        return sounds[name];
    }
    
    // Check if a sound exists
    bool hasSound(const std::string& name) const {
        return sounds.find(name) != sounds.end();
    }
    
    // Initialize with default assets
    bool loadDefaultAssets() {
        bool success = true;
        
        // Try to load textures
        success &= loadTexture("player", "assets/player.png");
        success &= loadTexture("coin", "assets/coin.png");
        success &= loadTexture("electric", "assets/electric.png");
        success &= loadTexture("background", "assets/background.png");
        
        // Try to load fonts
        success &= loadFont("main", "assets/font.ttf");
        
        // Try to load sounds
        success &= loadSound("jump", "assets/jump.wav");
        success &= loadSound("coin", "assets/coin.wav");
        success &= loadSound("death", "assets/death.wav");
        
        return success;
    }
    
private:
    std::map<std::string, sf::Texture> textures;
    std::map<std::string, sf::Font> fonts;
    std::map<std::string, sf::SoundBuffer> sounds;
};