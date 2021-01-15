#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>
#include "funct.h"

void errorExit(std::string err_funct){
    std::perror(err_funct.c_str());
    exit(EXIT_FAILURE);
}

int splitCmd(std::string cmd, std::vector<std::string>&arg){
    std::stringstream ss{cmd};
    std::string temp;
    int arg_count{};
    while (ss >> temp){
        arg.push_back(temp);
        arg_count += 1;

        // Long option value
        if (temp.compare(0, 2, "--") == 0){
            temp.clear();

            // Stop getting value if meet another long opt
            char c = ss.get();
            while ((c = ss.get()) != EOF){
                if ((c == '-') && (ss.peek() == '-')){
                    ss.unget();
                    temp.pop_back();
                    break;
                }
                else
                    temp.push_back(c);
            }

            arg.push_back(temp);
            arg_count += 1;
        }
        
        if (arg_count == 2 && !arg[0].compare("comment")){
            ss.get();
            arg_count += 1;
            std::getline(ss, temp);
            arg.push_back(temp);
        }
    }

    return arg_count;
}