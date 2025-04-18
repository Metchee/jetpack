#include "../shared_include/Client.hpp"
#include <fstream>
#include <iostream>
#include <cstdlib>

void ClientModule::Client::receiveAssets(int serverSocket)
{
    int command = 1;
    send(serverSocket, &command, sizeof(command), 0);
    
    auto files = receiveDirectoryData(serverSocket);
    
    // Display files to user
    for (const auto& file : files) {
        std::cout << file.name << " (" << file.size << " bytes)" << std::endl;
    }
    
    // Request files we need
    std::vector<std::string> requiredFiles = {
        "player1.png", "player2.png", "coin.png", "wall.png", 
        "electric.png", "finish.png", "background.png", "font.ttf"
    };
    
    for (const auto& fileName : requiredFiles) {
        // Check if file is available
        bool fileExists = false;
        for (const auto& file : files) {
            if (file.name == fileName) {
                fileExists = true;
                break;
            }
        }
        
        if (fileExists) {
            command = 2;
            send(serverSocket, &command, sizeof(command), 0);

            // Send filename
            size_t nameLength = fileName.size();
            send(serverSocket, &nameLength, sizeof(nameLength), 0);
            send(serverSocket, fileName.c_str(), nameLength, 0);

            // Receive and save file
            receiveFile(serverSocket, "assets/" + fileName);
        }
    }
}

void ClientModule::Client::receiveFile(int serverSocket, const std::string& savePath)
{
    // Create directories if needed
    size_t lastSlash = savePath.find_last_of('/');
    if (lastSlash != std::string::npos) {
        std::string dir = savePath.substr(0, lastSlash);
        // Simple directory creation (ignoring errors if directory exists)
        std::string mkdirCmd = "mkdir -p " + dir;
        system(mkdirCmd.c_str());
    }
    
    // Receive file size
    size_t fileSize;
    recv(serverSocket, &fileSize, sizeof(fileSize), 0);
    
    std::ofstream file(savePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to create file: " << savePath << std::endl;
        return;
    }
    
    // Receive file in chunks
    const size_t bufferSize = 4096;
    char buffer[bufferSize];
    size_t remaining = fileSize;
    
    while (remaining > 0) {
        size_t toRead = std::min(bufferSize, remaining);
        ssize_t bytesRead = recv(serverSocket, buffer, toRead, 0);
        
        if (bytesRead <= 0) {
            std::cerr << "Error receiving file data for " << savePath << std::endl;
            break;
        }
        
        file.write(buffer, bytesRead);
        remaining -= bytesRead;
    }
    
    file.close();
    
    if (debugMode) {
        std::cout << "File received successfully: " << savePath << " (" << fileSize << " bytes)" << std::endl;
    }
}

std::vector<ClientModule::Client::ReceivedFileInfo> ClientModule::Client::receiveDirectoryData(int serverSocket)
{
    std::vector<ReceivedFileInfo> files;
    
    // Receive number of files
    size_t fileCount;
    recv(serverSocket, &fileCount, sizeof(fileCount), 0);
    
    // Receive each file's info
    for (size_t i = 0; i < fileCount; ++i) {
        ReceivedFileInfo fileInfo;
        
        // Receive filename
        size_t nameLength;
        recv(serverSocket, &nameLength, sizeof(nameLength), 0);
        char* nameBuffer = new char[nameLength + 1];
        recv(serverSocket, nameBuffer, nameLength, 0);
        nameBuffer[nameLength] = '\0';
        fileInfo.name = nameBuffer;
        delete[] nameBuffer;
        
        // Receive file size
        recv(serverSocket, &fileInfo.size, sizeof(fileInfo.size), 0);
        
        files.push_back(fileInfo);
    }
    
    return files;
}