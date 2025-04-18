#pragma once
#include <stdbool.h>
#define ERROR 84
#define MAX_CLIENTS 10
#define FUNC_ERROR -1
#define SUCCESS 0
#define MAP_SIZE 1000
#include <exception>
#include <string>

class Exception : public std::exception {
  public:
    Exception(const std::string &message) : _message(message) {};
    const char *what() const noexcept override { return _message.c_str(); };
    ~Exception() = default;

  protected:
  private:
    std::string _message;
};


