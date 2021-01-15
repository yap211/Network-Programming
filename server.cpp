#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <unistd.h>
#include <sqlite3.h>
#include "funct.h"
#include "acc_manage.h"

#define BUF_SIZE 512
#define MAP_SIZE 128
#define DB_NAME "np.db"
#define SHM_PATH "/np_shm"

typedef struct {
    int key;
    std::string name;
} maps;

typedef struct {
    int num{};
    maps online[MAP_SIZE];
} stack_t;

stack_t *shmp;


void tcp_work(int fd);
void udp_work(int fd);
void shmInitialise();
void dbInitialise();

int main(int argc, char **argv){
    if (argc != 2){
        fprintf(stderr, "./server <port-number>\n");
        exit(EXIT_FAILURE);
    }

    // Set up shared memory
    //shmInitialise();

    // Set up database
    dbInitialise();

    // Address info
    sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(atoi(argv[1]));
    address.sin_addr.s_addr = INADDR_ANY;

    // Create, bind, and listen master socket(TCP socket)
    int master_socket{}, udpfd{};
    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        errorExit("[TCP]socket()", errno);

    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0)
        errorExit("[TCP]bind()", errno);

    if (listen(master_socket, 10) < 0)
        errorExit("[TCP]listen()", errno);

    // Create and bind UDP socket
    if ((udpfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        errorExit("[UDP]socket()", errno);

    if (bind(udpfd, (struct sockaddr *)&address, sizeof(address)) < 0)
        errorExit("[UDP]bind()", errno);

    // Fork a process for UDP connection
    int rtn{fork()};
    if (rtn == 0)
        udp_work(udpfd);
    else if (rtn < 0)
        errorExit("fork()", errno);

    // Pending connections and process requests
    sockaddr_in clnt_addr;
    socklen_t clnt_len;
    int connfd{};
    std::string welc_msg = "********************************\n"
                            "** Welcome to the BBS server. **\n"
                            "********************************\n";
    while (1){
        if ((connfd = accept(master_socket, (sockaddr *)&clnt_addr, &clnt_len)) < 0)
            fprintf(stderr, "accept(): %s\n", std::strerror(errno));
        else {
            std::cout << "New connection.\n";
            send(connfd, welc_msg.c_str(), welc_msg.size(), 0);
            if ((rtn = fork()) == 0)
                tcp_work(connfd);
        }
    }

    return 0;
}

void tcp_work(int fd){
    std::srand(std::time(0));
    char *buf = new char[BUF_SIZE];

    // Receive and process request
    std::string cmd, msg;
    std::vector<std::string> cmd_arg;
    int arg_count{}, key{};
    while (1){
        bzero(buf, BUF_SIZE);

        if (recv(fd, buf, BUF_SIZE, 0) < 0){
            fprintf(stderr, "recv(): %s\n", std::strerror(errno));
            continue;
        }

        cmd.assign(buf);
        arg_count = splitCmd(cmd, cmd_arg);
        if (cmd_arg[0].compare("login") == 0){
            // login
            if (checkLogin(cmd_arg[1], cmd_arg[2])){
                // Insert key
                key = insertLog(cmd_arg[1]);
                msg = std::to_string(key);
            }
            else {
                // Log in failed
                msg = "-1";
            }
        }
        else if (cmd_arg[0].compare("logout") == 0){
            // logout
            msg.assign("Bye, ");
            msg.append(retName(std::to_string(key)));
            msg.pop_back();
            msg.append(".");
            deleteLog(std::to_string(key));
        }
        else if (cmd_arg[0].compare("list-user") == 0){
            // list-user
            msg = listUser();
            if (msg.empty())
                msg.assign("\n");
        }
        else if (cmd_arg[0].compare("exit") == 0){
            // exit
            close(fd);
            break;
        }

        // Send reply back to client
        send(fd, msg.c_str(), msg.size(), 0);

        msg.clear();
        cmd_arg.clear();
    }

    delete [] buf;
}

void udp_work(int fd){
    char *buf = new char[BUF_SIZE];
    bzero(buf, BUF_SIZE);

    sockaddr_in clnt_addr;
    socklen_t clnt_len = sizeof(clnt_addr);

    // Receive and process request
    std::string cmd, msg;
    std::vector<std::string> cmd_arg;
    int arg_count{};
    while (1){
        if (recvfrom(fd, buf, BUF_SIZE, 0, (sockaddr *)&clnt_addr, &clnt_len) < 0){
            fprintf(stderr, "recvfrom(): %s\n", std::strerror(errno));
            continue;
        }
        
        cmd.assign(buf);
        arg_count = splitCmd(cmd, cmd_arg);
        if (cmd_arg[0].compare("register") == 0){
            // register
            int rtn{insertData(cmd_arg[1], cmd_arg[2], cmd_arg[3])};
            if (rtn < 0)
                msg.assign("Username is already used.");
            else
                msg.assign("Register successfully.");
        }

        else if (cmd_arg[0].compare("whoami") == 0){
            // whoami
            msg.assign(retName(cmd_arg[1]));
        }

        // Send reply back to client
        sendto(fd, msg.c_str(), msg.size(), 0, (sockaddr *)&clnt_addr, clnt_len);

        msg.clear();
        cmd_arg.clear();
        bzero(buf, BUF_SIZE);
    }

    delete [] buf;
}

void dbInitialise(){
    sqlite3 *db;
    char *err_msg;

    int rc = sqlite3_open(DB_NAME, &db);
    if (rc != SQLITE_OK){
        fprintf(stderr, "sqlite3_open(): %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(EXIT_FAILURE);
    }

    std::string sql = "DROP TABLE IF EXISTS USERINFO;"
                        "CREATE TABLE USERINFO("
                        "ID INTEGER PRIMARY KEY AUTOINCREMENT,"
                        "NAME VARCHAR(64) UNIQUE NOT NULL,"
                        "PASSWORD VARCHAR(64) NOT NULL,"
                        "EMAIL VARCHAR(64) NOT NULL);";
    rc = sqlite3_exec(db, sql.c_str(), 0, 0, &err_msg);
    if (rc != SQLITE_OK){
        fprintf(stderr, "sqlite3_exec(): %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        exit(EXIT_FAILURE);
    }

    sql = "DROP TABLE IF EXISTS LOG;"
            "CREATE TABLE LOG("
            "ID INTEGER UNIQUE NOT NULL,"
            "NAME VARCHAR(64) NOT NULL);";
    rc = sqlite3_exec(db, sql.c_str(), 0, 0, &err_msg);
    if (rc != SQLITE_OK){
        fprintf(stderr, "sqlite3_exec(): %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        exit(EXIT_FAILURE);
    }

    sqlite3_close(db);
}

void shmInitialise(){
    int shmfd = shm_open(SHM_PATH, O_RDWR | O_CREAT, S_IRWXU);
    if (shmfd < 0)
        errorExit("shm_open()", errno);

    shmp = (stack_t *)mmap(NULL, sizeof(shmp), PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    if (shmp == MAP_FAILED)
        errorExit("mmap()", errno);
}

