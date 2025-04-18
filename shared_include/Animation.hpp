#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <map>
#include <string>

class Animation {
public:
    Animation() : currentFrame(0), totalTime(0), frameTime(0.1f), isPlaying(false), isLooped(true) {}
    void addFrame(sf::IntRect rect) {
        frames.push_back(rect);
    }
    void setFrameTime(float time) {
        frameTime = time;
    }
    void setLoop(bool loop) {
        isLooped = loop;
    }
    void play() {
        isPlaying = true;
    }
    void stop() {
        isPlaying = false;
        currentFrame = 0;
        totalTime = 0;
    }
    void pause() {
        isPlaying = false;
    }
    void update(float deltaTime) {
        if (!isPlaying || frames.empty())
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
    sf::IntRect getCurrentFrame() const {
        if (frames.empty())
            return sf::IntRect();
        return frames[currentFrame];
    }
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
class AnimatedSprite : public sf::Sprite {
public:
    AnimatedSprite() : animation(nullptr), currentAnimation("") {}
    void addAnimation(const std::string& name, Animation animation) {
        animations[name] = animation;
    }
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
    void update(float deltaTime) {
        if (animation && animation->playing()) {
            animation->update(deltaTime);
            setTextureRect(animation->getCurrentFrame());
        }
    }
    bool isPlaying() const {
        return animation && animation->playing();
    }
    void stop() {
        if (animation)
            animation->stop();
    }

private:
    std::map<std::string, Animation> animations;
    Animation* animation;
    std::string currentAnimation;
};