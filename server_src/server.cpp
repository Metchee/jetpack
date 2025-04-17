#include "../shared_include/Server.hpp"
#include "../shared_include/Packet.hpp"
#include <iostream>
#include <fstream>
#include <sys/cdefs.h>
#include <sys/poll.h>
#include <unistd.h>
#include <getopt.h>
#include <memory>
#include <vector>
#include <arpa/inet.h>
#include <unordered_map>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <algorithm>

Server::Server(int argc, char* argv[]) 
    : _packetsUpdated(false), _serverFd(-1), _nbClients(1),
      _gameStarted(false), _gameWaitingForPlayers(true)
{
    // Analyser les arguments de la ligne de commande
    config.parseArgs(argc, argv);
    
    // Créer le socket serveur
    _serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (_serverFd < 0) {
        throw std::runtime_error("Failed to create socket");
    }
    
    // Configurer le socket pour réutiliser l'adresse
    int opt = 1;
    if (setsockopt(_serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(_serverFd);
        throw std::runtime_error("Failed to set socket options");
    }
    
    // Rendre le socket serveur non bloquant
    int flags = fcntl(_serverFd, F_GETFL, 0);
    fcntl(_serverFd, F_SETFL, flags | O_NONBLOCK);
    
    // Configurer l'adresse du serveur
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(config.port);
    
    // Lier le socket à l'adresse
    if (bind(_serverFd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        close(_serverFd);
        throw std::runtime_error("Failed to bind socket");
    }
    
    // Passer en mode écoute
    if (listen(_serverFd, 10) < 0) {
        close(_serverFd);
        throw std::runtime_error("Failed to listen on socket");
    }
    
    // Charger la carte depuis le fichier spécifié
    if (!_game.loadMap(config.map_file)) {
        close(_serverFd);
        throw std::runtime_error("Failed to load map file: " + config.map_file);
    }
    
    _lastUpdateTime = std::chrono::high_resolution_clock::now();
    
    if (config.debug_mode) {
        std::cout << "[SERVER] Server started on port " << config.port << std::endl;
        std::cout << "[SERVER] Map loaded successfully from " << config.map_file << std::endl;
        std::cout << "[SERVER] Waiting for players..." << std::endl;
    }
}

Server::~Server()
{
    stop();
}

void Server::stop()
{
    for (auto& client : _fdsList) {
        close(client->fd);
    }
    _fdsList.clear();
    
    if (_serverFd != -1) {
        close(_serverFd);
        _serverFd = -1;
    }
    
    if (config.debug_mode) {
        std::cout << "[SERVER] Server shutdown complete" << std::endl;
    }
}

void Server::run()
{
    std::vector<pollfd> poll_fds;
    pollfd server_pollfd;
    server_pollfd.fd = _serverFd;
    server_pollfd.events = POLLIN;
    poll_fds.push_back(server_pollfd);

    _fdsList.push_back(std::make_shared<pollfd>(server_pollfd));

    while (true) {
        poll_fds.clear();
        
        for (auto& client : _fdsList) {
            pollfd client_pollfd;
            client_pollfd.fd = client->fd;
            client_pollfd.events = POLLIN;
            poll_fds.push_back(client_pollfd);
        }
        
        // Poll avec timeout court pour permettre les mises à jour régulières du jeu
        int ready = poll(poll_fds.data(), poll_fds.size(), 16); // 16ms ~ 60fps
        
        // Mettre à jour l'état du jeu
        if (_gameStarted) {
            updateGame();
        } else if (_fdsList.size() >= 3 && _gameWaitingForPlayers) {
            // Démarrer le jeu si nous avons au moins 2 joueurs (+1 pour le serveur)
            startGame();
        }
        
        if (ready < 0) {
            if (config.debug_mode) {
                std::cerr << "[SERVER] Poll error: " << strerror(errno) << std::endl;
            }
            continue;
        }
        
        // Traiter les événements de poll
        for (size_t i = 0; i < poll_fds.size(); ++i) {
            if (poll_fds[i].revents & POLLIN) {
                if (poll_fds[i].fd == _serverFd) {
                    handleNewConnection();
                } else {
                    handleClientData(poll_fds[i].fd);
                }
            }
        }
        
        // Envoyer les mises à jour aux clients si nécessaire
        if (_packetsUpdated) {
            broadcastPackets();
        }
    }
}

void Server::handleNewConnection()
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    if (config.debug_mode) {
        std::cout << "[SERVER] Accepting new connection..." << std::endl;
    }
    
    int client_fd = accept(_serverFd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        if (config.debug_mode) {
            std::cerr << "[SERVER] Failed to accept new connection" << std::endl;
        }
        return;
    }
    
    // Rendre le socket non bloquant
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
    
    int client_id = _nbClients++;
    if (config.debug_mode) {
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        std::cout << "[SERVER] New connection from " << client_ip 
                  << ":" << ntohs(client_addr.sin_port) 
                  << " assigned ID: " << client_id 
                  << " (fd: " << client_fd << ")" << std::endl;
    }
    
    auto new_pollfd = std::make_shared<pollfd>();
    new_pollfd->fd = client_fd;
    new_pollfd->events = POLLIN;
    _fdsList.push_back(new_pollfd);
    
    // Associer l'ID client au file descriptor
    _clientIds[client_fd] = client_id - 1;  // -1 car le premier client a l'ID 0
    
    // Créer un nouveau paquet pour ce client
    PacketModule clientPacket(_nbClients - 1);
    clientPacket.getPacket().client_id = client_id - 1;
    _packets[client_id - 1] = clientPacket;
    
    // Initialiser le joueur dans le jeu
    if (_gameStarted && client_id - 1 < MAX_CLIENTS) {
        _game.getPlayerMutable(client_id - 1).alive = true;
    }
    
    _packetsUpdated = true;
}

void Server::broadcastPackets()
{
    if (_fdsList.size() <= 1) {  // S'il n'y a que le serveur, ne rien faire
        return;
    }
    
    // Pour chaque client connecté
    for (auto& client : _fdsList) {
        if (client->fd != _serverFd) {  // Ignorer le socket serveur
            // Trouver l'ID du client
            auto it = _clientIds.find(client->fd);
            if (it != _clientIds.end()) {
                int clientId = it->second;
                
                // Construire un paquet contenant l'état actuel du jeu
                PacketModule gameState(_nbClients - 1);
                
                // Copier l'état de tous les joueurs
                for (int i = 0; i < std::min(MAX_CLIENTS, _nbClients - 1); ++i) {
                    const auto& player = _game.getPlayer(i);
                    
                    // État du joueur
                    if (!player.alive) {
                        gameState.getPacket().playerState[i] = PacketModule::ENDED;
                    } else if (player.finished) {
                        gameState.getPacket().playerState[i] = (_game.getWinner() == i) 
                            ? PacketModule::WINNER 
                            : PacketModule::ENDED;
                    } else if (_gameStarted) {
                        gameState.getPacket().playerState[i] = PacketModule::PLAYING;
                    } else {
                        gameState.getPacket().playerState[i] = PacketModule::WAITING;
                    }
                    
                    // Position
                    gameState.getPacket().playerPosition[i] = std::make_pair(
                        static_cast<int>(player.x * 100), 
                        static_cast<int>(player.y * 100)
                    );
                    
                    // Score
                    gameState.getPacket().playerScore[i] = player.score;
                    
                    // Jetpack
                    gameState.getPacket().jetpackActive[i] = player.jetpack;
                }
                
                // Définir l'ID du client pour ce paquet
                gameState.getPacket().client_id = clientId;
                
                // Envoyer le paquet
                sendPacket(client->fd, gameState);
            }
        }
    }
    
    _packetsUpdated = false;
}

void Server::handleClientData(int client_fd)
{
    if (config.debug_mode) {
        std::cout << "[SERVER] Handling data from client " << client_fd << std::endl;
    }
    auto it = _clientIds.find(client_fd);
    if (it == _clientIds.end()) {
        removeClient(client_fd);
        return;
    }
    PacketModule pkt(_nbClients - 1);
    if (!readPacket(client_fd, pkt)) {
        removeClient(client_fd);
        return;
    }
    
    int clientId = it->second;
    
    // Traiter les entrées du joueur
    handlePlayerInput(clientId, pkt);
    
    if (config.debug_mode) {
        std::cout << "[SERVER] Successfully received packet from client " << clientId << std::endl;
    }
    
    _packets[clientId] = pkt;
    _packetsUpdated = true;
}

void Server::removeClient(int fd)
{
    // Trouver l'ID du client
    auto it = _clientIds.find(fd);
    if (it != _clientIds.end()) {
        int clientId = it->second;
        
        if (config.debug_mode) {
            std::cout << "[SERVER] Removing client " << clientId << " (fd: " << fd << ")" << std::endl;
        }
        
        // Marquer le joueur comme mort dans le jeu
        if (clientId < MAX_CLIENTS) {
            _game.getPlayerMutable(clientId).alive = false;
        }
        
        // Supprimer l'entrée dans la map des IDs
        _clientIds.erase(it);
    }
    
    // Fermer le socket
    close(fd);
    
    // Supprimer le file descriptor de la liste
    for (auto it = _fdsList.begin(); it != _fdsList.end(); ++it) {
        if ((*it)->fd == fd) {
            _fdsList.erase(it);
            break;
        }
    }
    
    // Mettre à jour l'état du jeu si tous les clients sont déconnectés
    if (_fdsList.size() <= 1 && _gameStarted) {  // S'il ne reste que le serveur
        _gameStarted = false;
        _gameWaitingForPlayers = true;
        if (config.debug_mode) {
            std::cout << "[SERVER] All clients disconnected, resetting game state" << std::endl;
        }
    }
    
    _packetsUpdated = true;
}

bool Server::readPacket(int client_fd, PacketModule& packetModule)
{
    PacketModule::Packet tmp;
    ssize_t bytesRead = recv(client_fd, &tmp, sizeof(PacketModule::Packet), MSG_DONTWAIT);
    
    if (bytesRead < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Pas de données disponibles, pas d'erreur
            return true;
        }
        if (config.debug_mode) {
            std::cerr << "[SERVER] Error reading from client (fd: " << client_fd 
                     << "): " << strerror(errno) << std::endl;
        }
        return false;
    } else if (bytesRead == 0) {
        // Connexion fermée
        if (config.debug_mode) {
            std::cout << "[SERVER] Client (fd: " << client_fd << ") disconnected" << std::endl;
        }
        return false;
    } else if (bytesRead != sizeof(PacketModule::Packet)) {
        // Lecture partielle ou erreur
        if (config.debug_mode) {
            std::cerr << "[SERVER] Partial read from client (fd: " << client_fd 
                     << "): got " << bytesRead << " bytes, expected " 
                     << sizeof(PacketModule::Packet) << std::endl;
        }
        return false;
    }
    
    // Lecture réussie
    packetModule.setPacket(tmp);
    
    return true;
}

int Server::sendPacket(int client_fd, PacketModule& packetModule)
{
    const PacketModule::Packet& pkt = packetModule.getPacket();
    ssize_t bytesSent = send(client_fd, &pkt, sizeof(pkt), MSG_NOSIGNAL);
    
    if (bytesSent < 0) {
        if (config.debug_mode) {
            std::cerr << "[SERVER] Error sending to client (fd: " << client_fd 
                     << "): " << strerror(errno) << std::endl;
        }
        return -1;
    } else if (bytesSent != sizeof(pkt)) {
        if (config.debug_mode) {
            std::cerr << "[SERVER] Partial send to client (fd: " << client_fd 
                     << "): sent " << bytesSent << " bytes, expected " 
                     << sizeof(pkt) << std::endl;
        }
        return -1;
    }
    
    return bytesSent;
}

void Server::startGame()
{
    if (_fdsList.size() < 3) {  // Moins de 2 clients + le serveur
        // On a besoin d'au moins 2 joueurs
        return;
    }
    
    _game.init();
    _gameStarted = true;
    _gameWaitingForPlayers = false;
    
    // Envoyer la carte à tous les clients
    for (auto& client : _fdsList) {
        if (client->fd != _serverFd) {
            sendMapToClient(client->fd);
        }
    }
    
    // Initialiser l'horodatage de mise à jour
    _lastUpdateTime = std::chrono::high_resolution_clock::now();
    
    if (config.debug_mode) {
        std::cout << "[SERVER] Game started with " << _fdsList.size() - 1 << " players" << std::endl;
    }
    
    // Mettre à jour les paquets pour indiquer que le jeu a commencé
    for (auto& pair : _packets) {
        auto& pkt = pair.second.getPacket();
        for (int i = 0; i < MAX_CLIENTS; i++) {
            pkt.playerState[i] = PacketModule::PLAYING;
        }
    }
    
    _packetsUpdated = true;
}

void Server::updateGame()
{
    // Calculer le delta time
    auto currentTime = std::chrono::high_resolution_clock::now();
    float deltaTime = std::chrono::duration<float>(currentTime - _lastUpdateTime).count();
    _lastUpdateTime = currentTime;
    
    // Limiter deltaTime pour éviter les sauts trop grands
    if (deltaTime > 0.1f) {
        deltaTime = 0.1f;
    }
    
    // Mettre à jour l'état du jeu
    _game.update(deltaTime);
    
    // Vérifier si le jeu est terminé
    if (_game.isGameOver()) {
        int winner = _game.getWinner();
        
        // Mettre à jour les paquets pour indiquer que le jeu est terminé
        for (auto& pair : _packets) {
            auto& pkt = pair.second.getPacket();
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (i == winner) {
                    pkt.playerState[i] = PacketModule::WINNER;
                } else {
                    pkt.playerState[i] = PacketModule::LOSER;
                }
            }
        }
        
        // Réinitialiser l'état du jeu pour la prochaine partie
        _gameStarted = false;
        _gameWaitingForPlayers = true;
        
        if (config.debug_mode) {
            if (winner != -1) {
                std::cout << "[SERVER] Game over. Player " << winner + 1 << " wins!" << std::endl;
            } else {
                std::cout << "[SERVER] Game over. No winner." << std::endl;
            }
        }
    }
    
    // Mettre à jour les paquets avec le nouvel état du jeu
    for (int i = 0; i < std::min(MAX_CLIENTS, _nbClients - 1); ++i) {
        const auto& player = _game.getPlayer(i);
        
        // Mettre à jour chaque client avec la position de tous les joueurs
        for (auto& pair : _packets) {
            auto& pkt = pair.second.getPacket();
            
            pkt.playerPosition[i] = std::make_pair(
                static_cast<int>(player.x * 100), // Multiplier par 100 pour conserver la précision
                static_cast<int>(player.y * 100)
            );
            
            pkt.playerScore[i] = player.score;
            pkt.jetpackActive[i] = player.jetpack;
        }
    }
    
    _packetsUpdated = true;
}

void Server::sendGameState()
{
    // Envoyer les paquets mis à jour à tous les clients
    broadcastPackets();
}

void Server::handlePlayerInput(int clientId, const PacketModule& packet)
{
    if (!_gameStarted || clientId < 0 || clientId >= MAX_CLIENTS) {
        return;
    }
    
    // Extraire les informations d'entrée du paquet
    bool jetpackActive = packet.getJetpackActive();
    
    // Mettre à jour le joueur
    auto currentTime = std::chrono::high_resolution_clock::now();
    float deltaTime = std::chrono::duration<float>(currentTime - _lastUpdateTime).count();
    
    // Limiter deltaTime pour éviter les sauts trop grands
    if (deltaTime > 0.1f) {
        deltaTime = 0.1f;
    }
    
    _game.updatePlayer(clientId, jetpackActive, deltaTime);
}

void Server::sendMapToClient(int clientFd)
{
    // Créer un paquet contenant la carte
    PacketModule mapPacket(_nbClients - 1);
    mapPacket.setPacketType(PacketModule::MAP_DATA);
    
    // Trouver l'ID du client
    auto it = _clientIds.find(clientFd);
    if (it == _clientIds.end()) {
        if (config.debug_mode) {
            std::cerr << "[SERVER] Cannot send map: client FD not found in ID map" << std::endl;
        }
        return;
    }
    
    int clientId = it->second;
    mapPacket.getPacket().client_id = clientId;
    
    // Envoyer le paquet
    sendPacket(clientFd, mapPacket);
    
    if (config.debug_mode) {
        std::cout << "[SERVER] Sent map to client " << clientId << std::endl;
    }
}