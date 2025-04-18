#include "../shared_include/Graphics.hpp"
#include <iostream>
#include <cstring>

namespace ClientModule {

Graphics::Graphics(std::shared_ptr<Game> game)
    : _window(nullptr), _game(game), _running(true) {
}

Graphics::~Graphics() {
    if (_window) {
        _window->close();
        delete _window;
    }
}

void Graphics::initialize() {
    // Create window
    _window = new sf::RenderWindow(sf::VideoMode(800, 600), "Jetpack Joyride");
    _window->setFramerateLimit(60);
    
    // Initialize game
    _game->init();
}

void Graphics::run() {
    if (!_window) {
        std::cerr << "Window not initialized!" << std::endl;
        return;
    }
    
    sf::Clock clock;
    
    while (_running && _window->isOpen()) {
        // Handle events
        sf::Event event;
        while (_window->pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                _window->close();
                _running = false;
            }
        }
        
        // Process input
        _game->processInput();
        
        // Update game
        float deltaTime = clock.restart().asSeconds();
        _game->update(deltaTime);
        
        // Render
        _game->render(*_window);
    }
}

void Graphics::stop() {
    _running = false;
    if (_window && _window->isOpen()) {
        _window->close();
    }
}

bool Graphics::isRunning() const {
    return _running && _window && _window->isOpen();
}

} // namespace ClientModule