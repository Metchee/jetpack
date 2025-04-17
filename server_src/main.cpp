#include "../shared_include/Server.hpp"
#include "../shared_include/Error.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        Server server(argc, argv);
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return ERROR;
    }
    return SUCCESS;
}
