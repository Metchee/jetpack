#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>
#include <memory>
#include <map>
#include <string>
#include <mutex>
#include "../shared_include/Game.hpp"
#include "../shared_include/Packet.hpp"

class GraphicsEngine {
public:
    GraphicsEngine();
    ~GraphicsEngine();

    // Initialiser la fenêtre SFML
    bool init(int windowWidth, int windowHeight, const std::string& title);

    // Charger les textures et les ressources
    bool loadResources();

    // Mettre à jour l'état du jeu affiché
    void updateGameState(const PacketModule& packet, std::mutex& mutex);

    // Définir la carte du jeu
    void setMap(const std::vector<std::vector<TileType>>& map);

    // Boucle principale de rendu
    void run();

    // Vérifier si la fenêtre est ouverte
    bool isOpen() const;

    // Obtenir l'entrée du joueur (active le jetpack ou non)
    bool getJetpackActive() const;

private:
    // Fenêtre SFML
    sf::RenderWindow _window;
    
    // Vue pour le défilement
    sf::View _gameView;
    
    // Taille de tuile
    const int TILE_SIZE = 32;
    
    // Dimensions
    int _windowWidth;
    int _windowHeight;
    
    // Ressources
    std::map<std::string, sf::Texture> _textures;
    std::map<TileType, sf::Sprite> _tileSprites;
    sf::Sprite _playerSprite;
    sf::Sprite _otherPlayerSprite;
    sf::Font _font;
    
    // État du jeu
    std::vector<std::vector<TileType>> _map;
    std::vector<std::pair<int, int>> _playerPositions;
    std::vector<bool> _playerAlive;
    std::vector<int> _playerScores;
    int _clientId;
    int _nbClients;
    PacketModule::gameState _gameState;
    bool _jetpackActive;
    
    // Verrou pour l'accès aux données partagées
    std::mutex* _mutex;
    
    // Méthodes de rendu
    void render();
    void drawMap();
    void drawPlayers();
    void drawUI();
    void handleEvents();
};