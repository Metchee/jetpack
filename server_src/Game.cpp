#include "../shared_include/Game.hpp"

Game::Game() : _width(0), _height(0), _gameStarted(false), _gameOver(false) {
    _players.resize(MAX_CLIENTS);
}

Game::~Game() {
}

bool Game::loadMap(const std::string& mapFile) {
    std::ifstream file(mapFile);
    if (!file.is_open()) {
        std::cerr << "Failed to open map file: " << mapFile << std::endl;
        return false;
    }
    
    // Lire les dimensions de la carte
    file >> _width >> _height;
    file.ignore(); // Ignorer le caractère de nouvelle ligne
    
    // Initialiser la carte avec les dimensions
    _map.resize(_height, std::vector<TileType>(_width, EMPTY));
    
    // Lire les tuiles de la carte
    std::string line;
    for (int y = 0; y < _height && std::getline(file, line); ++y) {
        for (int x = 0; x < _width && x < static_cast<int>(line.length()); ++x) {
            _map[y][x] = charToTile(line[x]);
            
            // Définir les positions de départ des joueurs
            if (_map[y][x] == START_P1) {
                _players[0].x = x;
                _players[0].y = y;
                _map[y][x] = EMPTY; // On efface le marqueur pour ne pas bloquer le joueur
            } else if (_map[y][x] == START_P2) {
                _players[1].x = x;
                _players[1].y = y;
                _map[y][x] = EMPTY; // On efface le marqueur pour ne pas bloquer le joueur
            }
        }
    }
    
    file.close();
    return true;
}

void Game::init() {
    // Réinitialiser les joueurs
    for (auto& player : _players) {
        player.score = 0;
        player.alive = true;
        player.finished = false;
        player.velX = 0;
        player.velY = 0;
        player.jetpack = false;
    }
    
    _gameStarted = true;
    _gameOver = false;
}

void Game::update(float deltaTime) {
    if (!_gameStarted || _gameOver) {
        return;
    }
    
    // Mettre à jour chaque joueur
    for (size_t i = 0; i < _players.size(); ++i) {
        if (_players[i].alive && !_players[i].finished) {
            // La mise à jour du joueur se fera via updatePlayer appelé avec les inputs
            
            // Vérifier les collisions après le déplacement
            checkCollisions(i);
        }
    }
    
    // Vérifier si la partie est terminée
    if (isGameOver()) {
        _gameOver = true;
    }
}

void Game::updatePlayer(int playerId, bool jetpackActive, float deltaTime) {
    if (playerId < 0 || playerId >= static_cast<int>(_players.size()) || !_players[playerId].alive || _players[playerId].finished) {
        return;
    }
    
    Player& player = _players[playerId];
    
    // Mettre à jour l'état du jetpack
    player.jetpack = jetpackActive;
    
    // Constantes physiques
    const float gravity = 20.0f;
    const float jetpackForce = 25.0f;
    const float horizontalSpeed = 5.0f;
    const float maxVerticalVelocity = 10.0f;
    
    // Déplacement horizontal constant
    player.velX = horizontalSpeed;
    
    // Appliquer la gravité ou la force du jetpack
    if (jetpackActive) {
        player.velY -= jetpackForce * deltaTime; // Le jetpack pousse vers le haut
    } else {
        player.velY += gravity * deltaTime; // La gravité tire vers le bas
    }
    
    // Limiter la vitesse verticale
    if (player.velY > maxVerticalVelocity) {
        player.velY = maxVerticalVelocity;
    } else if (player.velY < -maxVerticalVelocity) {
        player.velY = -maxVerticalVelocity;
    }
    
    // Calculer les nouvelles positions
    float newX = player.x + player.velX * deltaTime;
    float newY = player.y + player.velY * deltaTime;
    
    // Vérifier les limites de la carte
    if (newY < 0) {
        newY = 0;
        player.velY = 0;
    } else if (newY >= _height) {
        newY = _height - 0.1f;
        player.velY = 0;
    }
    
    // Mise à jour de la position horizontale (toujours valide)
    player.x = newX;
    
    // Vérifier les collisions avec les murs pour le mouvement vertical
    TileType tileAtNewPos = getTileAt(player.x, newY);
    if (tileAtNewPos != WALL) {
        player.y = newY;
    } else {
        // Arrêter le mouvement vertical si on touche un mur
        player.velY = 0;
        
        // Trouver la position valide la plus proche
        if (player.velY > 0) { // Mouvement vers le bas
            player.y = std::floor(player.y);
        } else { // Mouvement vers le haut
            player.y = std::ceil(player.y);
        }
    }
}

void Game::checkCollisions(int playerId) {
    if (playerId < 0 || playerId >= static_cast<int>(_players.size())) {
        return;
    }
    
    Player& player = _players[playerId];
    if (!player.alive || player.finished) {
        return;
    }
    
    // Vérifier le type de tuile à la position actuelle du joueur
    TileType tile = getTileAt(player.x, player.y);
    
    switch (tile) {
        case COIN:
            // Collecter la pièce
            player.score++;
            // Remplacer la pièce par un espace vide
            _map[static_cast<int>(player.y)][static_cast<int>(player.x)] = EMPTY;
            break;
            
        case ELECTRIC:
            // Le joueur meurt s'il touche une case électrique
            player.alive = false;
            break;
            
        case FINISH:
            // Le joueur a atteint la ligne d'arrivée
            player.finished = true;
            break;
            
        default:
            // Aucune action pour les autres types de tuiles
            break;
    }
}

bool Game::isGameOver() const {
    // La partie est terminée si tous les joueurs sont morts ou ont atteint l'arrivée
    bool allDoneOrDead = true;
    
    for (size_t i = 0; i < 2; ++i) { // On ne vérifie que les 2 premiers joueurs (minimum requis)
        if (_players[i].alive && !_players[i].finished) {
            allDoneOrDead = false;
            break;
        }
    }
    
    // Ou si un joueur est mort (l'autre gagne)
    bool playerDead = !_players[0].alive || !_players[1].alive;
    
    return allDoneOrDead || playerDead;
}

int Game::getWinner() const {
    if (!_gameOver) {
        return -1; // Pas de gagnant si la partie n'est pas terminée
    }
    
    // Si un joueur est mort, l'autre gagne
    if (!_players[0].alive) {
        return 1; // Joueur 2 gagne
    }
    if (!_players[1].alive) {
        return 0; // Joueur 1 gagne
    }
    
    // Si les deux ont atteint l'arrivée, celui avec le plus de score gagne
    if (_players[0].finished && _players[1].finished) {
        if (_players[0].score > _players[1].score) {
            return 0; // Joueur 1 gagne
        } else if (_players[1].score > _players[0].score) {
            return 1; // Joueur 2 gagne
        }
    }
    
    // Si un seul a atteint l'arrivée, il gagne
    if (_players[0].finished) {
        return 0; // Joueur 1 gagne
    }
    if (_players[1].finished) {
        return 1; // Joueur 2 gagne
    }
    
    // Si aucune des conditions précédentes n'est remplie, il n'y a pas de gagnant
    return -1;
}

TileType Game::charToTile(char c) {
    switch (c) {
        case '#': return WALL;
        case '$': return COIN;
        case 'E': return ELECTRIC;
        case 'S': return START_P1;
        case 's': return START_P2;
        case 'F': return FINISH;
        default: return EMPTY;
    }
}

bool Game::isValidPosition(float x, float y) const {
    int ix = static_cast<int>(x);
    int iy = static_cast<int>(y);
    
    return ix >= 0 && ix < _width && iy >= 0 && iy < _height;
}

TileType Game::getTileAt(float x, float y) const {
    int ix = static_cast<int>(x);
    int iy = static_cast<int>(y);
    
    if (!isValidPosition(x, y)) {
        return WALL; // Considérer les positions hors carte comme des murs
    }
    
    return _map[iy][ix];
}
