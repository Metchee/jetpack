#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <string>
#include <map>
#include <memory>
#include <iostream>

class AssetManager {
public:
    AssetManager() = default;
    ~AssetManager() = default;
    
    bool loadTexture(const std::string& name, const std::string& filename) {
        sf::Texture texture;
        if (!texture.loadFromFile(filename)) {
            std::cerr << "Failed to load texture: " << filename << std::endl;
            return false;
        }
        
        textures[name] = texture;
        return true;
    }
    
    sf::Texture& getTexture(const std::string& name) {
        return textures[name];
    }
    bool hasTexture(const std::string& name) const {
        return textures.find(name) != textures.end();
    }
    bool loadFont(const std::string& name, const std::string& filename) {
        sf::Font font;
        if (!font.loadFromFile(filename)) {
            std::cerr << "Failed to load font: " << filename << std::endl;
            return false;
        }
        
        fonts[name] = font;
        return true;
    }
    sf::Font& getFont(const std::string& name) {
        return fonts[name];
    }
    bool hasFont(const std::string& name) const {
        return fonts.find(name) != fonts.end();
    }
    bool loadSound(const std::string& name, const std::string& filename) {
        sf::SoundBuffer buffer;
        if (!buffer.loadFromFile(filename)) {
            std::cerr << "Failed to load sound: " << filename << std::endl;
            return false;
        }
        
        sounds[name] = buffer;
        return true;
    }
    sf::SoundBuffer& getSound(const std::string& name) {
        return sounds[name];
    }
    bool hasSound(const std::string& name) const {
        return sounds.find(name) != sounds.end();
    }
    bool loadDefaultAssets() {
        bool success = true;
        success &= loadTexture("player", "assets/player_sprite_sheet.png");
        success &= loadTexture("coin", "assets/coins_sprite_sheet.png");
        success &= loadTexture("electric", "assets/zapper_sprite_sheet.png");
        success &= loadTexture("background", "assets/background.png");
        success &= loadFont("main", "assets/jetpack_font.ttf");
        success &= loadSound("jump", "assets/jetpack_start.wav");
        success &= loadSound("coin", "assets/coin_pickup_1.wav");
        success &= loadSound("death", "assets/dud_zapper_pop.wav");
        success &= loadSound("jetpack", "assets/jetpack_lp.wav");
        success &= loadSound("jetpack_stop", "assets/jetpack_stop.wav");
        
        return success;
    }
private:
    std::map<std::string, sf::Texture> textures;
    std::map<std::string, sf::Font> fonts;
    std::map<std::string, sf::SoundBuffer> sounds;
};