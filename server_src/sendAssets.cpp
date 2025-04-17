#include <iostream>
#include <fstream>
#include <filesystem>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <cstring>
#include <arpa/inet.h>
#include "Server.hpp"

std::vector<Server::FileInfo> Server::getDirectoryList(const std::string &directoryPath)
{
    std::vector<Server::FileInfo> files;

    try {
        for (const auto& entry : std::filesystem::directory_iterator(directoryPath)) {
            if (entry.is_regular_file()) {
                files.push_back({
                    entry.path().filename().string(),
                    entry.path().string(),
                    entry.file_size()
                });
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }
    return files;
}

void Server::sendDirectoryList(int clientSocket, const std::string& directoryPath)
{
    auto files = getDirectoryList(directoryPath);
    size_t fileCount = files.size();

    send(clientSocket, &fileCount, sizeof(fileCount), 0);
    for (const auto& file : files) {
        size_t nameLength = file.name.size();
        send(clientSocket, &nameLength, sizeof(nameLength), 0);
        send(clientSocket, file.name.c_str(), nameLength, 0);
        send(clientSocket, &file.size, sizeof(file.size), 0);
    }
}

void Server::sendFile(int clientSocket, const std::string& filePath)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filePath << std::endl;
        return;
    }
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    send(clientSocket, &fileSize, sizeof(fileSize), 0);
    const size_t bufferSize = 4096;
    char buffer[bufferSize];
    size_t remaining = fileSize;

    while (remaining > 0) {
        size_t toRead = std::min(bufferSize, remaining);
        file.read(buffer, toRead);
        send(clientSocket, buffer, toRead, 0);
        remaining -= toRead;
    }
    file.close();
}
