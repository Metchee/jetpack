#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <cmath>

// Classe pour gérer les animations de sprites
class Animation {
public:
    Animation() : currentFrame(0), totalTime(0), frameTime(0.1f), isPlaying(false), isLooped(true) {}

    // Ajouter un frame à l'animation
    void addFrame(sf::IntRect rect) {
        frames.push_back(rect);
    }

    // Définir la durée de chaque frame
    void setFrameTime(float time) {
        frameTime = time;
    }

    // Définir si l'animation doit boucler
    void setLoop(bool loop) {
        isLooped = loop;
    }

    // Démarrer l'animation
    void play() {
        isPlaying = true;
    }

    // Arrêter l'animation
    void stop() {
        isPlaying = false;
        currentFrame = 0;
        totalTime = 0;
    }

    // Pauser l'animation
    void pause() {
        isPlaying = false;
    }

    // Mettre à jour l'animation avec le temps écoulé
    void update(float deltaTime) {
        if (!isPlaying)
            return;

        totalTime += deltaTime;

        if (totalTime >= frameTime) {
            totalTime -= frameTime;
            if (currentFrame + 1 < frames.size())
                currentFrame++;
            else {
                if (isLooped)
                    currentFrame = 0;
                else {
                    currentFrame = frames.size() - 1;
                    stop();
                }
            }
        }
    }

    // Obtenir le rectangle de texture actuel
    sf::IntRect getCurrentFrame() const {
        return frames[currentFrame];
    }

    // Vérifier si l'animation est en cours
    bool playing() const {
        return isPlaying;
    }

private:
    std::vector<sf::IntRect> frames;
    size_t currentFrame;
    float totalTime;
    float frameTime;
    bool isPlaying;
    bool isLooped;
};

// Classe de gestionnaire d'animation pour un sprite
class AnimatedSprite : public sf::Sprite {
public:
    AnimatedSprite() : animation(nullptr), currentAnimation("") {}

    // Ajouter une animation
    void addAnimation(const std::string& name, Animation animation) {
        animations[name] = animation;
    }

    // Jouer une animation
    void play(const std::string& name) {
        if (currentAnimation != name) {
            if (animations.find(name) != animations.end()) {
                currentAnimation = name;
                animation = &animations[name];
                animation->play();
                setTextureRect(animation->getCurrentFrame());
            }
        }
    }

    // Mettre à jour l'animation
    void update(float deltaTime) {
        if (animation && animation->playing()) {
            animation->update(deltaTime);
            setTextureRect(animation->getCurrentFrame());
        }
    }

    // Vérifier si une animation est en cours
    bool isPlaying() const {
        return animation && animation->playing();
    }

    // Arrêter l'animation courante
    void stop() {
        if (animation)
            animation->stop();
    }

private:
    std::map<std::string, Animation> animations;
    Animation* animation;
    std::string currentAnimation;
};