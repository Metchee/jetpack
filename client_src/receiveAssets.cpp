#include "Client.hpp"
#include <fstream>

void ClientModule::Client::receiveAssets(int serverSocket)
{
    int command = 1;
    send(serverSocket, &command, sizeof(command), 0);
    
    auto files = receiveDirectoryData(serverSocket);
    
    // Display files to user
    for (const auto& file : files) {
        std::cout << file.name << " (" << file.size << " bytes)" << std::endl;
    }
    
    // Request a file
    std::string selectedFile[] = {"player.png","map.png"};
    int nb_files = 2;
    for (int i = 0; i < nb_files; i++) {
        command = 2;
        send(serverSocket, &command, sizeof(command), 0);

        // Send filename
        size_t nameLength = selectedFile[i].size();
        send(serverSocket, &nameLength, sizeof(nameLength), 0);
        send(serverSocket, selectedFile[i].c_str(), nameLength, 0);

        // Receive and save file
        receiveFile(serverSocket, "downloaded_" + selectedFile[i]);
    }
}

void ClientModule::Client::receiveFile(int serverSocket, const std::string& savePath)
{
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
        recv(serverSocket, buffer, toRead, 0);
        file.write(buffer, toRead);
        remaining -= toRead;
    }
    
    file.close();
    std::cout << "File received successfully: " << savePath << std::endl;    
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

