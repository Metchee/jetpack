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


Server::Server(int argc, char* argv[]) 
    : _packetsUpdated(false), _serverFd(-1), _nbClients(1),
      _gameStarted(false), _gameWaitingForPlayers(true)
{
    // ... code existant ...
    
    // Charger la carte depuis le fichier spécifié
    if (!_game.loadMap(config.map_file)) {
        throw std::runtime_error("Failed to load map file: " + config.map_file);
    }
    
    _lastUpdateTime = std::chrono::high_resolution_clock::now();
    
    if (config.debug_mode) {
        std::cout << "[SERVER] Map loaded successfully from " << config.map_file << std::endl;
    }
}

void Server::run()
{
    std::vector<pollfd> poll_fds;
    pollfd server_pollfd;
    server_pollfd.fd = _serverFd;
    server_pollfd.events = POLLIN;
    poll_fds.push_back(server_pollfd);

    while (true) {
        poll_fds.resize(1); 
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
        } else if (_fdsList.size() >= 2 && _gameWaitingForPlayers) {
            // Démarrer le jeu si nous avons au moins 2 joueurs
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

void Server::startGame()
{
    if (_fdsList.size() < 2) {
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
        pkt.playerState[pair.first] = PacketModule::PLAYING;
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
            pkt.playerState[pair.first] = PacketModule::ENDED;
            
            // Marquer le gagnant si applicable
            if (winner != -1 && pair.first == winner) {
                // On pourrait ajouter un état spécial pour le gagnant
                // Pour l'instant, on garde simplement ENDED
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
    for (auto& pair : _packets) {
        auto clientId = pair.first;
        auto& pkt = pair.second.getPacket();
        
        // Mettre à jour la position du joueur
        const auto& player = _game.getPlayer(clientId);
        pkt.playerPosition[clientId] = std::make_pair(
            static_cast<int>(player.x * 100), // Multiplier par 100 pour conserver la précision
            static_cast<int>(player.y * 100)
        );
        
        // Mettre à jour le score du joueur (si on ajoute un champ score au paquet)
        // pkt.playerScore[clientId] = player.score;
    }
    
    _packetsUpdated = true;
    
    // Envoyer l'état du jeu aux clients
    sendGameState();
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
    // NOTE: On pourrait ajouter un champ jetpackActive au paquet
    // Pour l'instant, on utilise une valeur factice
    bool jetpackActive = false; // À remplacer par packet.getJetpackActive();
    
    // Mettre à jour le joueur
    _game.updatePlayer(clientId, jetpackActive, 0.016f); // 16ms ~ 60fps
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
    PacketModule pkt(_nbClients);
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
    
    _packets.insert_or_assign(clientId, pkt);
    _packetsUpdated = true;
}

void Server::sendMapToClient(int clientFd)
{
    // TODO: Implémenter l'envoi de la carte au client
    // Cette méthode devrait envoyer les données de la carte au client spécifié
    // Elle pourrait utiliser un paquet spécial ou un format de données personnalisé
    
    // Exemple simple:
    PacketModule mapPacket(_nbClients);
    auto& pkt = mapPacket.getPacket();
    
    // On pourrait ajouter des champs spécifiques à la carte dans le paquet
    // Pour l'instant, on marque simplement l'état du joueur comme WAITING
    pkt.playerState[_clientIds[clientFd]] = PacketModule::WAITING;
    
    sendPacket(clientFd, mapPacket);
    
    if (config.debug_mode) {
        std::cout << "[SERVER] Sent map to client " << _clientIds[clientFd] << std::endl;
    }
}