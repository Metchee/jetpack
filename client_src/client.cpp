#include "../shared_include/Client.hpp"
#include <cstdio>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <SFML/Graphics.hpp>

void ClientModule::Client::parseArguments(int argc, const char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" && i + 1 < argc) {
            serverIp = argv[++i];
        } else if (arg == "-p" && i + 1 < argc) {
            serverPort = std::stoi(argv[++i]);
        } else if (arg == "-d") {
            debugMode = true;
        }
    }
}

ClientModule::Client::Client(int ac, const char *av[]) :
    fd(-1), id(-1), serverPort(-1), serverIp(""), connected(false),
    window(sf::VideoMode(800, 600), "Jetpack Game"),
    mapWidth(0), mapHeight(0)
{
    parseArguments(ac, av);
    if (!font.loadFromFile("assets/jetpack_font.ttf")) {
        throw std::runtime_error("Failed to load font");
    }
    if (!playerTexture.loadFromFile("assets/player_sprite_sheet.png")) {
        throw std::runtime_error("Failed to load player texture");
    }
    if (!coinTexture.loadFromFile("assets/coins_sprite_sheet.png")) {
        throw std::runtime_error("Failed to load coin texture");
    }
    if (!zapperTexture.loadFromFile("assets/zapper_sprite_sheet.png")) {
        throw std::runtime_error("Failed to load zapper texture");
    }
    playerSprite.setTexture(playerTexture);
    coinSprite.setTexture(coinTexture);
    zapperSprite.setTexture(zapperTexture);
    playerSprite.setScale(0.5f, 0.5f);
    coinSprite.setScale(0.5f, 0.5f);
    zapperSprite.setScale(0.5f, 0.5f);
    window.setFramerateLimit(60);
}

ClientModule::Client::~Client() {
    if (connected) {
        close(fd);
    }
    window.close();
}

void ClientModule::Client::run()
{
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throw std::runtime_error("Failed to create socket");
    }
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    if (inet_pton(AF_INET, serverIp.c_str(), &serverAddr.sin_addr) <= 0) {
        throw std::runtime_error("Invalid server address");
    }
    if (connect(fd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        throw std::runtime_error("Connection failed");
    }
    connected = true;
    address = serverAddr;
    if (debugMode) {
        std::cout << "Connected to server at " << getAddress() << std::endl;
    }
    startThread();
    runThread();
}

void ClientModule::Client::stop() {
    connected = false;
    window.close();
}

std::string ClientModule::Client::getAddress() const {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &address.sin_addr, ip, INET_ADDRSTRLEN);
    return std::string(ip) + ":" + std::to_string(ntohs(address.sin_port));
}

void ClientModule::Client::gameThread()
{
    auto parseMap = [&](const char* mapData) {
        std::stringstream ss(mapData);
        std::string line;
        map.clear();
        while (std::getline(ss, line)) {
            std::vector<char> row(line.begin(), line.end());
            map.push_back(row);
        }
        mapHeight = map.size();
        mapWidth = map.empty() ? 0 : map[0].size();
    };

    while (connected && window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                stop();
            }
        }

        PacketModule current_state;
        {
            std::lock_guard<std::mutex> lock(_packetMutex);
            current_state.setPacket(packet.getPacket());
        }
        if (map.empty()) {
            parseMap(current_state.getPacket().map);
        }

        window.clear(sf::Color::Black);

        const float tileSize = 32.0f;
        for (int y = 0; y < mapHeight; ++y) {
            for (int x = 0; x < mapWidth; ++x) {
                char tile = map[y][x];
                if (tile == 'C') { 
                    coinSprite.setPosition(x * tileSize, y * tileSize);
                    window.draw(coinSprite);
                } else if (tile == 'E') {
                    zapperSprite.setPosition(x * tileSize, y * tileSize);
                    window.draw(zapperSprite);
                }
            }
        }

        for (int i = 0; i < current_state.getNbClient(); ++i) {
            if (current_state.getPacket().playerState[i] == PacketModule::PLAYING) {
                auto pos = current_state.getPacket().playerPosition[i];
                playerSprite.setPosition(pos.first * tileSize, pos.second * tileSize);
                window.draw(playerSprite);
            }
        }

        sf::Text debugText;
        debugText.setFont(font);
        debugText.setCharacterSize(20);
        debugText.setFillColor(sf::Color::White);
        std::stringstream debugInfo;
        debugInfo << "Client ID: " << current_state.getClientId() << "\n"
                  << "Players: " << current_state.getNbClient() << "\n"
                  << "State: " << current_state.getstate() << "\n"
                  << "Position: (" << current_state.getPosition().first << ", "
                  << current_state.getPosition().second << ")";
        debugText.setString(debugInfo.str());
        debugText.setPosition(10, 10);
        if (debugMode) {
            window.draw(debugText);
        }

        if (current_state.getstate() == PacketModule::ENDED) {
            sf::Text endText;
            endText.setFont(font);
            endText.setCharacterSize(30);
            endText.setFillColor(sf::Color::Red);
            endText.setString("Game Over");
            endText.setPosition(window.getSize().x / 2 - 100, window.getSize().y / 2);
            window.draw(endText);
        }

        window.display();
    }
}

void ClientModule::Client::networkThread()
{
    PacketModule newGameState;

    while (connected) {
        if (recv(fd, &newGameState, sizeof(PacketModule::Packet), MSG_DONTWAIT) == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                connected = false;
            }
        }
        if (debugMode) {
            std::lock_guard<std::mutex> lock(_packetMutex);
            newGameState.display("[CLIENT]: Recv: ");
        }
        {
            std::lock_guard<std::mutex> lock(_packetMutex);
            packet.setPacket(newGameState.getPacket());
        }
        PacketModule outgoingPacket;
        {
            std::lock_guard<std::mutex> lock(_packetMutex);
            outgoingPacket.setPacket(packet.getPacket());
        }
        if (send(fd, &outgoingPacket.getPacket(), sizeof(PacketModule::Packet), MSG_DONTWAIT) == -1) {
            connected = false;
        }
        if (debugMode) {
            std::lock_guard<std::mutex> lock(_packetMutex);
            outgoingPacket.display("[CLIENT]: Send: ");
        }
    }
}

void ClientModule::Client::startThread()
{
    try {
        _gameThread = std::thread(&Client::gameThread, this);
        _networkThread = std::thread(&Client::networkThread, this);
    } catch (const std::system_error& e) {
        std::cerr << "Thread startup failed: " << e.what() << "\n";
        connected = false;
    }
}

void ClientModule::Client::runThread()
{
    if (_gameThread.joinable()) {
        _gameThread.join();
    }
    if (_networkThread.joinable()) {
        _networkThread.join();
    }
}