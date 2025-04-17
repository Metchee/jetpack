#pragma once

#include <vector>
#include <string>
#include <utility>
#include <fstream>
#include <iostream>
#include <cmath>
#include <memory>
#include "Error.hpp"

// Représente les différents types de tuiles dans le jeu
enum TileType {
    EMPTY = 0,    // Espace vide
    WALL,         // Mur/obstacle
    COIN,         // Pièce à collecter
    ELECTRIC,     // Case électrique (tue le joueur)
    START_P1,     // Point de départ du joueur 1
    START_P2,     // Point de départ du joueur 2
    FINISH        // Ligne d'arrivée
};

// Structure pour représenter un joueur
struct Player {
    float x;      // Position X
    float y;      // Position Y
    float velX;   // Vélocité X
    float velY;   // Vélocité Y
    bool jetpack; // Jetpack activé ?
    int score;    // Score du joueur
    bool alive;   // Joueur en vie ?
    bool finished;// A atteint la ligne d'arrivée ?
    
    Player() : x(0), y(0), velX(0), velY(0), jetpack(false), 
               score(0), alive(true), finished(false) {}
};

// Classe représentant le jeu Jetpack
class Game {
public:
    Game();
    ~Game();
    
    // Charge une carte depuis un fichier
    bool loadMap(const std::string& mapFile);
    
    // Initialise le jeu
    void init();
    
    // Met à jour l'état du jeu
    void update(float deltaTime);
    
    // Met à jour un joueur spécifique
    void updatePlayer(int playerId, bool jetpackActive, float deltaTime);
    
    // Vérifie les collisions et gère les conséquences
    void checkCollisions(int playerId);
    
    // Vérifie si la partie est terminée
    bool isGameOver() const;
    
    // Détermine le gagnant (retourne -1 s'il n'y a pas de gagnant)
    int getWinner() const;
    
    // Accesseurs
    const std::vector<std::vector<TileType>>& getMap() const { return _map; }
    int getWidth() const { return _width; }
    int getHeight() const { return _height; }
    const Player& getPlayer(int id) const { return _players[id]; }
    Player& getPlayerMutable(int id) { return _players[id]; }
    
private:
    std::vector<std::vector<TileType>> _map;
    std::vector<Player> _players;
    int _width;
    int _height;
    bool _gameStarted;
    bool _gameOver;
    
    // Convertit un caractère en type de tuile
    TileType charToTile(char c);
    
    // Vérifie si une position est valide sur la carte
    bool isValidPosition(float x, float y) const;
    
    // Obtient le type de tuile à une position donnée
    TileType getTileAt(float x, float y) const;
};
