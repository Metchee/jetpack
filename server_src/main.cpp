#include "../shared_include/Client.hpp"
#include "../shared_include/Error.hpp"
#include <iostream>

int main(int argc, const char* argv[])
{
    try {
        // Convert const char** to char** (safe since Client won't modify argv)
        char** non_const_argv = const_cast<char**>(argv);
        ClientModule::Client client(argc, non_const_argv);
        client.run();
    } catch (const std::exception& e) {
        std::cerr << "Client error: " << e.what() << std::endl;
        return ERROR;
    }
    return SUCCESS;
}