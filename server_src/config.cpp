#include "../shared_include/Config.hpp"
#include <stdexcept>
#include <iostream>
#include <unistd.h>
#include <getopt.h>

void ServerConfig::parseArgs(int argc, char* argv[])
{
    int opt;
    while ((opt = getopt(argc, argv, "p:m:d")) != -1) {
        switch (opt) {
            case 'p':
                port = parsePort(optarg);
                break;
            case 'm':
                map_file = optarg;
                break;
            case 'd':
                debug_mode = true;
                break;
            case '?':
                printUsage(argv[0]);
                throw std::runtime_error("Invalid arguments");
            default:  printUsage(argv[0]);
                throw std::runtime_error("Argument parsing failed");
        }
    }
    if (optind < argc) {
        printUsage(argv[0]);
        throw std::runtime_error("Unexpected arguments");
    }
    validate();
}

int ServerConfig::parsePort(const std::string& port_str)
{
    try {
        int port = std::stoi(port_str);
        if (port < 1 || port > 65535) {
            throw std::out_of_range("Port out of range");
        }
        return port;
    } catch (const std::exception&) {
        throw std::runtime_error("Invalid port number: " + port_str);
    }
}

void ServerConfig::validate() const
{
    if (map_file.empty()) {
        throw std::runtime_error("Map file cannot be empty");
    }
}

void ServerConfig::printUsage(const std::string& program_name)
{
    std::cerr << "Usage: " << program_name 
              << " -p <port> -m <map> [-d]\n"
              << "Options:\n"
              << "  -p <port>    Server port (1-65535)\n"
              << "  -m <map>     Map file to load\n"
              << "  -d           Enable debug mode\n";
}
