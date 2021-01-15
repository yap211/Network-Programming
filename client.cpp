#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <string>
#include <vector>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "funct.h"

#define BUF_SIZE 512

int main(int argc, char **argv){
    if (argc != 3){
        fprintf(stderr, "./client <ip-address> <port-number>\n");
        exit(EXIT_FAILURE);
    }

    sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    socklen_t serv_len = sizeof(serv_addr);

    // Create TCP socket and connect to server
    int tcpfd{}, udpfd{};
    if ((tcpfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)    
        errorExit("[TCP]create()", errno);

    if (connect(tcpfd, (sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        errorExit("[TCP]connect()", errno);
    
    // Receive welcome message
    char *buf = new char[BUF_SIZE];
    bzero(buf, BUF_SIZE);
    if (recv(tcpfd, buf, BUF_SIZE, 0) < 0)
        errorExit("[TCP]read()", errno);
    else 
        std::cout << buf << "\n";

    // Create UDP socket
    if ((udpfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        errorExit("[UDP]create()", errno);

    // Send request
    std::string cmd;
    int id{-1};
    while (1){
        bzero(buf, BUF_SIZE);

        std::cout << "& ";
        std::getline(std::cin, cmd);

        std::vector<std::string> cmd_arg;
        int arg_count{splitCmd(cmd, cmd_arg)};

        /// Account related request ///
        if (cmd_arg[0].compare("register") == 0){
            // register
            if (arg_count != 4){
                fprintf(stderr, "register <username> <email> <password>\n");
                continue;
            }
            sendto(udpfd, cmd.c_str(), cmd.size(), 0, (struct sockaddr *)&serv_addr, serv_len);
            recvfrom(udpfd, buf, BUF_SIZE, 0, (struct sockaddr *)&serv_addr, &serv_len);

            std::cout << buf << "\n";
        }
        else if (cmd_arg[0].compare("login") == 0){
            // login
            if (arg_count != 3){
                fprintf(stderr, "login: <username> <password>\n");
                continue;
            }
            if (id != -1){
                std::cout << "Please logout first.\n";
                continue;
            }

            send(tcpfd, cmd.c_str(), cmd.size(), 0);
            recv(tcpfd, buf, BUF_SIZE, 0);

            int loginStatus{atoi(buf)};
            if (loginStatus == -1)
                std::cout << "Login failed.\n";
            else{
                id = loginStatus;
                std::cout << "Welcome " << cmd_arg[1] << ".\n";
            }
        }
        else if (cmd_arg[0].compare("logout") == 0){
            // logout
            if (arg_count != 1){
                fprintf(stderr, "logout: Too many arguments\n");
                continue;
            }
            if (id == -1){
                std::cout << "Please login first.\n";
                continue;
            }

            id = -1;
            send(tcpfd, cmd.c_str(), cmd.size(), 0);
            recv(tcpfd, buf, BUF_SIZE, 0);

            std::cout << buf << "\n";
        }
        else if (cmd_arg[0].compare("whoami") == 0){
            // whoami
            if (arg_count != 1){
                fprintf(stderr, "whoami: Too many arguments\n");
                continue;
            }
            if (id == -1){
                std::cout << "Please login first.\n";
                continue;
            }

            cmd.append(" ").append(std::to_string(id));
            sendto(udpfd, cmd.c_str(), cmd.size(), 0, (struct sockaddr *)&serv_addr, serv_len);
            recvfrom(udpfd, buf, BUF_SIZE, 0, (struct sockaddr *)&serv_addr, &serv_len);

            std::cout << buf;
        }
        else if (cmd_arg[0].compare("list-user") == 0){
            // list-user
            if (arg_count != 1){
                fprintf(stderr, "list-user: Too many arguments\n");
                continue;
            }
            send(tcpfd, cmd.c_str(), cmd.size(), 0);
            recv(tcpfd, buf, BUF_SIZE, 0);
            std::cout << "Name\tEmail\n" << buf;
        }
        else if (cmd_arg[0].compare("exit") == 0){
            // exit
            if (arg_count != 1){
                fprintf(stderr, "exit: Too many arguments\n");
                continue;
            }
            send(tcpfd, cmd.c_str(), cmd.size(), 0);
            break;
        }
        else {
            std::string usage = "Usage:\n"
                                "register <username> <email> <password>\n"
                                "login <username> <password>\n"
                                "logout\nwhoami\nlist-user\nexit\n";
            std::cerr << usage;
        }
    }

    delete [] buf;

    return 0;
}
