#pragma once
#include <string>

struct ServerConfig {
    int port = 4242;
    std::string map_file = "default.map";
    bool debug_mode = false;
    
    void parseArgs(int argc, char* argv[]);
    void validate() const;

    private:
        static int parsePort(const std::string& port_str);
        static void printUsage(const std::string& program_name);
};