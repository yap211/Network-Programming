#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <netdb.h>
#include <semaphore.h>
#include <string>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#include "funct.h"

#define BUF_SIZE 512

struct Chatinfo{
    int first;
    int num;
    char comment[3][128];
    sem_t lock;
};

int chatroom_serv_init(uint16_t room_port);
void room_serv(uint16_t room_port, std::string owner);
int room_clnt(uint16_t room_port , std::string username);

Chatinfo *chatinfo;

int main(int argc, char **argv){
    if (argc != 3){
        fprintf(stderr, "./client <ip-address> <port-number>\n");
        exit(EXIT_FAILURE);
    }

    // Server address
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
    std::string cmd, username;
    int id{-1}, chatroom_stat{-1};
    uint16_t room_port{};
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
                username.assign(cmd_arg[1]);
                std::cout << "Welcome " << cmd_arg[1] << ".\n";

                // Create shared memory and initialise
                std::string shm_path = "/np_shm_" + username;
                int shmfd{shm_open(shm_path.c_str(), O_RDWR | O_CREAT, S_IRWXU)};
                ftruncate(shmfd, sizeof(Chatinfo));
                chatinfo = (Chatinfo *)mmap(NULL, sizeof(Chatinfo), PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
                chatinfo->first = 0;
                chatinfo->num = 0;
                sem_init(&chatinfo->lock, 1, 0);
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
            if (chatroom_stat == 1){
                std::cout << "Please do \"attach\" and \"leave-chatroom\" first." << std::endl;
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

            std::cout << buf << std::endl;
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

        /// BBS related ///
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

        /// Chatroom related //
        else if (cmd_arg[0].compare("create-chatroom") == 0){
            if (arg_count != 2){
                fprintf(stderr, "create-chatroom <post>\n");
                continue;
            }
            if (id == -1){
                std::cout << "Please login first." << std::endl;
                continue;
            }

            send(tcpfd, cmd.c_str(), cmd.size(), 0);
            recv(tcpfd, buf, BUF_SIZE, 0);
            
            // Server return -1 if user has already created the chatroom
            if (atoi(buf) == -1){
                std::cout << "User has already created the chatroom." << std::endl;
                continue;
            }

            // Create chatroom
            std::cout << "start to create chatroom..." << std::endl;

            room_port = htons(atoi(cmd_arg[1].c_str()));
            chatroom_stat = 1;

            // Fork a server and enter chatroom mode
            int rtn{fork()};
            if (rtn == 0){
                // Room server
                room_serv(room_port, username);
                _exit(0);
            }
            else {
                sem_wait(&chatinfo->lock);
                // Enter client, return after calling leave-chatroom or detach
                if (room_clnt(room_port, username) == 0){
                    // leave-chatroom, change chatroom status to 0, indicates close chatroom
                    chatroom_stat = 0;
                    cmd.assign("leave-chatroom");
                    send(tcpfd, cmd.c_str(), cmd.size(), 0);
                }
                else {
                    // Detach
                    // Do nothing, remain chatroom status to 1, indicates that chatroom is still open
                }
                std::cout << "Welcome back to BBS." << std::endl;
            }
            
        }

        else if (cmd_arg[0].compare("list-chatroom") == 0){
            if (arg_count != 1){
                fprintf(stderr, "list-chatroom: Too many arguments.\n");
                continue;
            }
            if (id == -1){
                std::cout << "Please login first." << std::endl;
                continue;
            }

            sendto(udpfd, cmd.c_str(), cmd.size(), 0, (struct sockaddr *)&serv_addr, serv_len);
            recvfrom(udpfd, buf, BUF_SIZE, 0, (struct sockaddr *)&serv_addr, &serv_len);

            std::cout << "Chatroom_name\tStatus" << std::endl;
            std::cout << buf << std::endl;
        }

        else if (cmd_arg[0].compare("join-chatroom") == 0){
            if (arg_count != 2){
                fprintf(stderr, "join-chatroom <chatroom_name>\n");
                continue;
            }
            if (id == -1){
                std::cout << "Please login first." << std::endl;
                continue;
            }

            send(tcpfd, cmd.c_str(), cmd.size(), 0);
            recv(tcpfd, buf, BUF_SIZE, 0);

            // Return 0 if chatroom does not exist or is close, else return port number
            int port = atoi(buf);
            if (port == 0)
                std::cout << "The chatroom does not exist or the chatroom is close." << std::endl;
            else {
                room_clnt(htons(port), username);
                std::cout << "Welcome back to BBS." << std::endl;
            }
        }

        else if (cmd_arg[0].compare("attach") == 0){
            if (arg_count != 1){
                fprintf(stderr, "attach: Too many arguments.\n");
                continue;
            }
            if (id == -1){
                std::cout << "Please login first." << std::endl;
                continue;
            }
            if (chatroom_stat == -1){
                std::cout << "Please create-chatroom first.\n" << std::endl;
                continue;
            }
            if (chatroom_stat == 0){
                std::cout << "Please restart-chatroom first.\n" << std::endl;
                continue;
            }

            int ret{room_clnt(room_port, username)};
            if (ret == 0){
                // leave-chatroom, change chatroom status to 0, indicates close chatroom
                chatroom_stat = 0;
                cmd.assign("leave-chatroom");
                send(tcpfd, cmd.c_str(), cmd.size(), 0);
            }
            else {
                // Detach
                // Do nothing, remain chatroom status to 1, indicates that chatroom is still open
            }
            std::cout << "Welcome back to BBS." << std::endl;
        }

        else if (cmd_arg[0].compare("restart-chatroom") == 0){
            if (arg_count != 1){
                std::cout << "restart-chatroom: Too many arguments." << std::endl;
                continue;
            }
            if (id == -1){
                std::cout << "Please login first." << std::endl;
                continue;
            }
            if (chatroom_stat == -1){
                std::cout << "Please create-chatroom first." << std::endl;
                continue;
            }
            if (chatroom_stat == 1){
                std::cout << "Your chatroom is still running." << std::endl;
                continue;
            }

            // Restart chatroom
            std::cout << "start to create chatroom..." << std::endl;

            send(tcpfd, cmd.c_str(), cmd.size(), 0);
            chatroom_stat = 1;

            // Fork a server and enter chatroom mode
            int rtn{fork()};
            if (rtn == 0){
                // Room server
                room_serv(room_port, username);
                _exit(0);
            }
            else {
                sem_wait(&chatinfo->lock);
                // Enter client, return after calling leave-chatroom or detach
                if (room_clnt(room_port, username) == 0){
                    // leave-chatroom, change chatroom status to 0, indicates close chatroom
                    chatroom_stat = 0;
                    cmd.assign("leave-chatroom");
                    send(tcpfd, cmd.c_str(), cmd.size(), 0);
                }
                else {
                    // Detach
                    // Do nothing, remain chatroom status to 1, indicates that chatroom is still open
                }
                 std::cout << "Welcome back to BBS." << std::endl;
            }
            
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

int chatroom_serv_init(uint16_t room_port){
    // Initialise TCP room server
    sockaddr_in room_addr;
    bzero(&room_addr, sizeof(room_addr));
    room_addr.sin_family = AF_INET;
    room_addr.sin_port = room_port;
    room_addr.sin_addr.s_addr = INADDR_ANY;

    int opt{1};
    int sfd{socket(AF_INET, SOCK_STREAM, 0)};
    if (sfd < 0){
        std::perror("[Chatroom]socket()");
        return -1;
    }
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
        std::perror("[Chatroom]setsockopt()");
        return -1;
    }
    if (bind(sfd, (struct sockaddr *)&room_addr, sizeof(room_addr)) < 0){
        std::perror("[Chatroom]bind()");
        return -1;
    }
    if (listen(sfd, 5) < 0){
        std::perror("[Chatroom]listen()");
        return -1;
    }

    return sfd;
}

void room_serv(uint16_t room_port, std::string owner){
    // Initialise room server
    int sfd{chatroom_serv_init(room_port)};
    sem_post(&chatinfo->lock);

    if (sfd == -1)
        return;

    sockaddr_in clnt_addr;
    socklen_t clnt_len = sizeof(clnt_addr);
    int connfd{};
    std::string welc_msg = "*************************\n"
                            "*Welcome to the chatroom*\n"
                            "*************************\n";

    fd_set read_fds, active_fds;
    int max_fds{sfd};
    std::unordered_map<int, std::string> cfd;
    FD_ZERO(&active_fds);
    FD_SET(sfd, &active_fds);

    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500;

    char *buf = new char[BUF_SIZE];
    std::time_t t;
    std::tm tm;
    std::string msg, hour, min;
    while (1){
        read_fds = active_fds;
        if (select(max_fds + 1, &read_fds, NULL, NULL, &tv) < 0){
            std::perror("[Room Server]select()");
            return;
        }

        // Check master socket for new connection
        if (FD_ISSET(sfd, &read_fds)){
            if ((connfd = accept(sfd, (sockaddr *)&clnt_addr, &clnt_len)) < 0)
                std::perror("[Room Server]accept()");
            else {
                bzero(buf, BUF_SIZE);

                // Append last 3 messages after welcome message
                msg = welc_msg;
                int first{chatinfo->first};
                for (int i = 0; i < chatinfo->num; ++i){
                    msg.append(chatinfo->comment[first]);
                    msg.append("\n");
                    first = (first + 1) % 3;
                }

                // Send welcome message to new connection
                send(connfd, msg.c_str(), msg.size(), 0);
                FD_SET(connfd, &active_fds);
                max_fds = connfd > max_fds ? connfd : max_fds; 

                // Receive username of new connection
                recv(connfd, buf, BUF_SIZE, 0);

                // Get current time
                t = std::time(nullptr);
                tm = *std::localtime(&t);
                hour.assign(std::to_string(tm.tm_hour));
                min.assign(std::to_string(tm.tm_min));
                if (min.size() == 1)
                    min = "0" + min;

                // Send new user enter message to other users if the new user isn't room owner
                if (owner.compare(buf)){
                    msg = "sys[" + hour + ":" + min + "]: " + buf + " joins us.";
                    for (auto &fd : cfd){
                        send(fd.first, msg.c_str(), msg.size(), 0);
                    }
                }
                // Insert socket number and username into unordered map
                cfd.insert({connfd, buf});
            }
        }

        // Check client socket input
        for (auto it = cfd.begin(); it != cfd.end(); ){
            if (FD_ISSET(it->first, &read_fds)){
                bzero(buf, BUF_SIZE);

                // Receive message
                recv(it->first, buf, BUF_SIZE, 0);

                // Get current time
                t = std::time(nullptr);
                tm = *std::localtime(&t);
                hour.assign(std::to_string(tm.tm_hour));
                min.assign(std::to_string(tm.tm_min));
                if (min.size() == 1)
                    min = "0" + min;

                // Check if the message is "leave-chatroom" and the sender is chatroom owner
                if (!strncmp(buf, "leave-chatroom", 14) && !it->second.compare(owner)){
                    // Send message to all connected client that server are closing
                    // Then close the socket
                    msg.assign("leave-chatroom");
                    for (auto &fd : cfd){
                        send(fd.first, msg.c_str(), msg.size(), 0);
                        close(fd.first);
                    }

                    // Close server socket
                    delete [] buf;
                    FD_ZERO(&active_fds);
                    close(sfd);
                    return;
                }
                else if (!strncmp(buf, "detach", 6) && !it->second.compare(owner)){
                    // Send to the client to close the socket
                    msg.assign(buf);
                    send(it->first, msg.c_str(), msg.size(), 0);

                    // Remove from the unordered map and active set
                    FD_CLR(it->first, &active_fds);
                    close(it->first);
                    it = cfd.erase(it);
                    continue;
                }
                else if (!strncmp(buf, "leave-chatroom", 14)){
                    // Send message to other that the sender has left the room
                    msg = "sys[" + hour + ":" + min + "]: " + it->second + " leaves us.";
                    for (auto &fd : cfd){
                        if (fd.first != it->first)
                            send(fd.first, msg.c_str(), msg.size(), 0);
                    }
                        
                    // Send to the sender client to close the socket
                    msg.assign(buf);
                    send(it->first, msg.c_str(), msg.size(), 0);

                    // Remove from the unordered map and active set
                    FD_CLR(it->first, &active_fds);
                    close(it->first);
                    it = cfd.erase(it);
                    continue;
                }
                else {
                    // Send the message to others
                    msg = it->second + "[" + hour + ":" + min + "]: " + buf;
                    if (chatinfo->num < 3){
                        bzero(chatinfo->comment[chatinfo->first], 128);
                        strcpy(chatinfo->comment[chatinfo->num], msg.c_str());
                        chatinfo->num += 1;
                    }
                    else {
                        bzero(chatinfo->comment[chatinfo->first], 128);
                        strcpy(chatinfo->comment[chatinfo->first], msg.c_str());
                        chatinfo->first = (chatinfo->first + 1) % 3;
                    }

                    for (auto &fd : cfd){
                        if (fd.first != it->first)
                            send(fd.first, msg.c_str(), msg.size(), 0);
                    }
                }
            }
            it++;
        }
    }
}

int room_clnt(uint16_t room_port, std::string username){
    sockaddr_in room_addr;
    bzero(&room_addr, sizeof(room_addr));
    room_addr.sin_family = AF_INET;
    room_addr.sin_port = room_port;
    room_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    // Connect to room server
    int fd{socket(AF_INET, SOCK_STREAM, 0)};
    if (fd < 0){
        std::perror("[Room Client]socket()");
        return -1;
    }
    if (connect(fd, (sockaddr *)&room_addr, sizeof(room_addr)) < 0){
        std::perror("[Room Client]connect()");
        return -1;
    }
    
    // Receive welcome message
    char *buf = new char[BUF_SIZE];
    bzero(buf, BUF_SIZE);
    if (recv(fd, buf, BUF_SIZE, 0) < 0)
        std::perror("[Room Client]recv()");
    else {
        std::cout << buf;

        // Send username to room server
        send(fd, username.c_str(), username.size(), 0);
    }

    fd_set read_fds, active_fds;
    int max_fds = fd;
    FD_ZERO(&active_fds);
    FD_SET(STDIN_FILENO, &active_fds);
    FD_SET(fd, &active_fds);

    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500;
    
    std::string msg;
    while (1){
        read_fds = active_fds;

        if (select(max_fds + 1, &read_fds, NULL, NULL, &tv) < 0){
            std::perror("[Room Client]select()");
            return -1;
        }

        // Check user input or server incoming message
        if (FD_ISSET(STDIN_FILENO, &read_fds)){
            // User input and send message to server
            std::getline(std::cin, msg);
            send(fd, msg.c_str(), msg.size(), 0);
        }
        else if (FD_ISSET(fd, &read_fds)){
            // Server message
            bzero(buf, BUF_SIZE);
            recv(fd, buf, BUF_SIZE, 0);

            // If server send leave-chatroom, close socket and return 0
            // If server send detach, close socket and return 1
            if (!strncmp(buf, "leave-chatroom", 14)){
                delete [] buf;
                close(fd);
                FD_ZERO(&active_fds);
                return 0;
            }
            else if (!strncmp(buf, "detach", 6)){
                delete [] buf;
                close(fd);
                FD_ZERO(&active_fds);
                return 1;
            }
            else
                std::cout << buf << std::endl;
        }
    }

    return 0;
}