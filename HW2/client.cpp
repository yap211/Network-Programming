#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
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
        errorExit("[TCP]create()");

    if (connect(tcpfd, (sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        errorExit("[TCP]connect()");
    
    // Receive welcome message
    char *buf = new char[BUF_SIZE];
    bzero(buf, BUF_SIZE);
    if (recv(tcpfd, buf, BUF_SIZE, 0) < 0)
        errorExit("[TCP]read()");
    else 
        std::cout << buf << "\n";

    // Create UDP socket
    if ((udpfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        errorExit("[UDP]create()");

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
            if (arg_count != 4){
                fprintf(stderr, "register <username> <email> <password>\n");
                continue;
            }
            sendto(udpfd, cmd.c_str(), cmd.size(), 0, (struct sockaddr *)&serv_addr, serv_len);
            recvfrom(udpfd, buf, BUF_SIZE, 0, (struct sockaddr *)&serv_addr, &serv_len);

            std::cout << buf << std::endl;
        }
        else if (cmd_arg[0].compare("login") == 0){
            if (arg_count != 3){
                fprintf(stderr, "login <username> <password>\n");
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
                std::cout << "Login failed." << std::endl;
            else{
                id = loginStatus;
                std::cout << "Welcome " << cmd_arg[1] << ".\n";
            }
        }
        else if (cmd_arg[0].compare("logout") == 0){
            if (arg_count != 1){
                fprintf(stderr, "logout: Too many arguments\n");
                continue;
            }
            if (id == -1){
                std::cout << "Please login first." << std::endl;
                continue;
            }

            id = -1;
            send(tcpfd, cmd.c_str(), cmd.size(), 0);
            recv(tcpfd, buf, BUF_SIZE, 0);

            std::cout << buf << std::endl;
        }
        else if (cmd_arg[0].compare("whoami") == 0){
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
            if (arg_count != 1){
                fprintf(stderr, "list-user: Too many arguments\n");
                continue;
            }

            send(tcpfd, cmd.c_str(), cmd.size(), 0);
            recv(tcpfd, buf, BUF_SIZE, 0);
            std::cout << "Name\tEmail\n" << buf;
        }

        else if (cmd_arg[0].compare("exit") == 0){
            if (arg_count != 1){
                fprintf(stderr, "exit: Too many arguments\n");
                continue;
            }

            send(tcpfd, cmd.c_str(), cmd.size(), 0);
            close(tcpfd);
            break;
        }

        else if (cmd_arg[0].compare("create-board") == 0){
            if (arg_count != 2){
                fprintf(stderr, "create-board <name>\n");
                continue;
            }

            if (id == -1){
                std::cout << "Please login first." << std::endl;
                continue;
            }

            send(tcpfd, cmd.c_str(), cmd.size(), 0);
            recv(tcpfd, buf, BUF_SIZE, 0);
            std::cout << buf << std::endl;
        }

        else if (cmd_arg[0].compare("create-post") == 0){
            if (arg_count != 6 || cmd_arg[2].compare("--title") || cmd_arg[4].compare("--content")){
                fprintf(stderr, "create-post <board-name> --title <title> --content <content>\n");
                continue;
            }

            if (id == -1){
                std::cout << "Please login first." << std::endl;
                continue;
            }

            send(tcpfd, cmd.c_str(), cmd.size(), 0);
            recv(tcpfd, buf, BUF_SIZE, 0);
            std::cout << buf << std::endl;
        }

        else if (cmd_arg[0].compare("list-board") == 0){
            if (arg_count != 1){
                fprintf(stderr, "list-board: Too many arguments\n");
                continue;
            }

            send(tcpfd, cmd.c_str(), cmd.size(), 0);
            recv(tcpfd, buf, BUF_SIZE, 0);
            std::cout << "Index\tName\tModerator\n" << buf;
        }

        else if (cmd_arg[0].compare("list-post") == 0){
            if (arg_count != 2){
                fprintf(stderr, "list-post <board-name>\n");
                continue;
            }

            send(tcpfd, cmd.c_str(), cmd.size(), 0);
            recv(tcpfd, buf, BUF_SIZE, 0);
            if (buf[0] == '0')
                std::cout << "Board does not exist." << std::endl;
            else
                std::cout << "S/N\tTitle\tAuthor\tDate\n" << buf;
        }

        else if (cmd_arg[0].compare("read") == 0){
            if (arg_count != 2){
                fprintf(stderr, "read <post-S/N>\n");
                continue;
            }

            send(tcpfd, cmd.c_str(), cmd.size(), 0);
            recv(tcpfd, buf, BUF_SIZE, 0);
            std::cout << buf << std::endl;
        }

        else if (cmd_arg[0].compare("delete-post") == 0){
            if (arg_count != 2){
                fprintf(stderr, "delete-post <post-S/N>\n");
                continue;
            }
            if (id == -1){
                std::cout << "Please login first." << std::endl;
                continue;
            }

            send(tcpfd, cmd.c_str(), cmd.size(), 0);
            recv(tcpfd, buf, BUF_SIZE, 0);
            std::cout << buf << std::endl;
        }

        else if (cmd_arg[0].compare("update-post") == 0){
            if ((arg_count != 4) || 
                (cmd_arg[2].compare("--title") && cmd_arg[2].compare("--content"))){
                fprintf(stderr, "update-post <post-S/N> --title/content <new>\n");
                continue;
            }
            if (id == -1){
                std::cout << "Please login first." << std::endl;
                continue;
            }

            send(tcpfd, cmd.c_str(), cmd.size(), 0);
            recv(tcpfd, buf, BUF_SIZE, 0);
            std::cout << buf << std::endl;
        }

        else if (cmd_arg[0].compare("comment") == 0){
            if (arg_count != 3){
                fprintf(stderr, "comment <post-S/N> <comment>\n");
                continue;
            }
            if (id == -1){
                std::cout << "Please login first." << std::endl;
                continue;
            }

            send(tcpfd, cmd.c_str(), cmd.size(), 0);
            recv(tcpfd, buf, BUF_SIZE, 0);
            std::cout << buf << std::endl;
        }

        else {
            std::string usage = "Usage:\n"
                                "[Account]\n"
                                "register <username> <email> <password>\n"
                                "login <username> <password>\n"
                                "logout\nwhoami\nlist-user\nexit\n\n"
                                "[BBS]\n"
                                "create-board <name>\n"
                                "create-post <board-name> --title <title> --content <content>\n"
                                "list-board\nlist-post <board-name>\n"
                                "read <post-S/N>\ndelete-post <post-S/N>\n"
                                "update-post <post-S/N> --title/content <new>\n"
                                "comment <post-S/N> <comment>\n";
            std::cerr << usage;
        }
    }

    delete [] buf;

    return 0;
}
