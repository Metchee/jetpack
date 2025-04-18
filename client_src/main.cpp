#include "Client.hpp"
#include "Error.hpp"
#include <iostream>

int main(int argc, const char* argv[])
{
    try {
        ClientModule::Client client(argc, argv);
        client.run();
    } catch (const std::exception& e) {
        std::cerr << "Client error: " << e.what() << std::endl;
        return ERROR;
    }
    return SUCCESS;
}
