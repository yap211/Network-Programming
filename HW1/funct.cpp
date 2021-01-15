#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <sstream>
#include "funct.h"


void errorExit(std::string err_funct, int errnum){
    fprintf(stderr, "%s: %s\n", err_funct.c_str(), std::strerror(errnum));
    exit(EXIT_FAILURE);
}

int splitCmd(std::string cmd, std::vector<std::string>&arg){
    std::stringstream ss(cmd);
    std::string temp;
    int arg_count{};
    while (ss >> temp){
        arg.push_back(temp);
        arg_count += 1;
    }

    return arg_count;
}
