#pragma once

#include <SFML/Graphics.hpp>
#include <memory>
#include "Game.hpp"

namespace ClientModule {

class Graphics {
public:
    Graphics(std::shared_ptr<Game> game);
    ~Graphics();
    
    void initialize();
    void run();
    void stop();
    bool isRunning() const;
    
private:
    sf::RenderWindow* _window;
    std::shared_ptr<Game> _game;
    bool _running;
};

} // namespace ClientModule