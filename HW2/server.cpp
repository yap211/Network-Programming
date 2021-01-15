#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <netdb.h>
#include <semaphore.h>
#include <sqlite3.h>
#include <string>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include "acc_manage.h"
#include "board_manage.h"
#include "funct.h"

#define BUF_SIZE 512
#define MAP_SIZE 128
#define DB_NAME "np.db"
#define SHM_PATH "/np_shm"

struct Comment{
    char user[64][64];
    char comment[64][64];
};

struct Post{
    int sn[64]{};
    int num_comment[64]{};  // Number of comment in a post
    char title[64][32];
    char content[64][64];
    char author[64][64];
    int tm_mday[64];
    int tm_mon[64];
    Comment user_comment[64];
    sem_t lock;
};

struct Board{
    sem_t lock;
    int sn_post{};  // Global post serial number
    int num_post[64]{}; // Number of post in a board
    Post post[64];
};

void tcp_work(int fd);
void udp_work(int fd);
void dbInitialise();
int tcpInitialise(sockaddr_in serv_addr);
int udpInitialise(sockaddr_in serv_addr);

Board *board;

int main(int argc, char **argv){
    if (argc != 2){
        fprintf(stderr, "./server <port-number>\n");
        exit(EXIT_FAILURE);
    }

    // Set up database
    dbInitialise();

    // Create shared memory and initialise
    int shmfd{shm_open(SHM_PATH, O_RDWR | O_CREAT, S_IRWXU)};
    ftruncate(shmfd, sizeof(Board));
    board = (Board *)mmap(NULL, sizeof(Board), PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    board->sn_post = 0;
    sem_init(&board->lock, 1, 1);
    for (int i = 0; i < 64; ++i){
        sem_init(&(board->post[i].lock), 1, 1);
        board->num_post[i] = 0;
        for (int j = 0; j < 64; ++j)
            board->post[i].num_comment[j] = 0;
    }

    // Address info
    sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[1]));
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    // Create, bind, and listen master socket(TCP socket) and UDP socket
    int master_socket{}, udpfd{};
    master_socket = tcpInitialise(serv_addr);    
    udpfd = udpInitialise(serv_addr);
    
    // Fork a process for UDP connection
    int rtn{fork()};
    if (rtn == 0)
        udp_work(udpfd);
    else if (rtn < 0)
        errorExit("fork()");

    // Pending connections and process requests
    sockaddr_in clnt_addr;
    socklen_t clnt_len = sizeof(clnt_addr);
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
    int arg_count{}, key{-1};
    while (1){
        bzero(buf, BUF_SIZE);

        if (recv(fd, buf, BUF_SIZE, 0) < 0){
            fprintf(stderr, "recv(): %s\n", std::strerror(errno));
            continue;
        }

        cmd.assign(buf);
        arg_count = splitCmd(cmd, cmd_arg);
        if (cmd_arg[0].compare("login") == 0){
            if (checkLogin(cmd_arg[1], cmd_arg[2])){
                // Insert key
                key = insertLog(cmd_arg[1], fd);
                msg = std::to_string(key);
            }
            else {
                // Log in failed
                msg = "-1";
            }
        }

        else if (cmd_arg[0].compare("logout") == 0){
            msg.assign("Bye, ");
            msg.append(retName(std::to_string(key)));
            msg.pop_back();
            msg.append(".");
            deleteLog(std::to_string(key));
            key = -1;
        }

        else if (cmd_arg[0].compare("list-user") == 0){
            msg = listUser();
            if (msg.empty())
                msg.assign("\n");
        }

        else if (cmd_arg[0].compare("exit") == 0){
            close(fd);
            break;
        }

        else if (cmd_arg[0].compare("create-board") == 0){
            sem_wait(&board->lock);
            std::string username = retName(std::to_string(key));
            username.pop_back();
            if (insertBoard(cmd_arg[1], username) == 0)
                msg.assign("Create board successfully.");
            else
                msg.assign("Board already exists.");
            sem_post(&board->lock);
        }

        else if (cmd_arg[0].compare("create-post") == 0){
            std::string username = retName(std::to_string(key));
            username.pop_back();
            int board_id{checkBoard(cmd_arg[1])};
            if (board_id == -1)
                msg.assign("Board does not exist.");
            else {
                sem_wait(&board->post[board_id].lock);
                int num = board->num_post[board_id];
                board->sn_post += 1;
                board->num_post[board_id] = num + 1;
                board->post[board_id].sn[num] = board->sn_post;
                strcpy(board->post[board_id].title[num], cmd_arg[3].c_str());
                strcpy(board->post[board_id].content[num], cmd_arg[5].c_str());
                strcpy(board->post[board_id].author[num], username.c_str());
                std::time_t t{std::time(nullptr)};
                std::tm tm = *std::localtime(&t);
                board->post[board_id].tm_mday[num] = tm.tm_mday;
                board->post[board_id].tm_mon[num] = tm.tm_mon;
                sem_post(&board->post[board_id].lock);
                msg.assign("Create post successfully.");
            }   
                
        }

        else if (cmd_arg[0].compare("list-board") == 0){
            sem_wait(&board->lock);
            msg = listBoard();
            if (msg.empty())
                msg.assign("\n");
            sem_post(&board->lock);
        }

        else if (cmd_arg[0].compare("list-post") == 0){
            int board_id{checkBoard(cmd_arg[1])};
            if (board_id == -1)
                msg.assign("0");    // Board does not exist
            else {
                sem_wait(&board->post[board_id].lock);
                //sn title author date
                int post_num = board->num_post[board_id];
                int sn{}, day{}, month{};
                char *title, *author;
                std::string temp;
                if (post_num == 0)
                    msg.assign("\n");
                else {
                    for (int i = 0; i < post_num; ++i){
                        if (board->post[board_id].sn[i] == -1){
                            post_num += 1;
                            continue;
                        }
                        else {
                            sn = board->post[board_id].sn[i];
                            day = board->post[board_id].tm_mday[i];
                            month = board->post[board_id].tm_mon[i] + 1;
                            title = board->post[board_id].title[i];
                            author = board->post[board_id].author[i];
                            temp = std::to_string(sn) + "\t" + title + "\t" + author + "\t" + 
                                    std::to_string(month) + "/" + std::to_string(day) + "\n";
                            msg.append(temp);
                        }
                    }
                }
                sem_post(&board->post[board_id].lock);
            }
        }

        else if (cmd_arg[0].compare("read") == 0){
            int sn = atoi(cmd_arg[1].c_str());
            // Check if the post exists
            for (int i = 1; i < 64; ++i){
                sem_wait(&board->post[i].lock);
                int num_post = board->num_post[i];
                if (num_post != 0){
                    for (int j = 0; j < 64; ++j){
                        if (board->post[i].sn[j] == -1){
                            num_post += 1;
                            continue;
                        }
                        // Match
                        if (sn == board->post[i].sn[j]){
                            std::string author, title, date, content;
                            author = board->post[i].author[j];
                            title = board->post[i].title[j];
                            date = std::to_string(board->post[i].tm_mday[j]) + "/" + std::to_string(board->post[i].tm_mon[j] + 1);
                            content = processContent(board->post[i].content[j]);
                            msg = "Author: " + author
                                    + "\nTitle: " + title
                                    + "\nDate: " + date
                                    + "\n--\n"
                                    + content
                                    + "\n--\n";
                            int num_comment = board->post[i].num_comment[j];
                            // Get comment
                            for (int k = 0; k < num_comment; ++k){
                                std::string user, comment;
                                user = board->post[i].user_comment[j].user[k];
                                comment = board->post[i].user_comment[j].comment[k];
                                msg.append(user);
                                msg.append(": ");
                                msg.append(comment);
                                msg.append("\n");
                            }
                        }
                    }
                    if (msg.size() > 0){
                        sem_post(&board->post[i].lock);
                        break;
                    }
                }
                sem_post(&board->post[i].lock);
            }
            if (msg.size() == 0)
                msg.assign("Post does not exist.");
        }

        else if (cmd_arg[0].compare("delete-post") == 0){
            int sn = atoi(cmd_arg[1].c_str());
            // Check if the post exist or not
            for (int i = 1; i < 64; ++i){
                sem_wait(&board->post[i].lock);
                int num_post = board->num_post[i];
                if (num_post != 0){
                    for (int j = 0; j < num_post; ++j){
                        if (board->post[i].sn[j] == -1){
                            num_post += 1;
                            continue;
                        }
                        if (sn == board->post[i].sn[j]){
                            // Check if the post owner
                            std::string username = retName(std::to_string(key));
                            username.pop_back();
                            if (username.compare(board->post[i].author[j]) != 0){
                                msg.assign("Not the post owner.");
                                break;
                            }

                            board->num_post[i] -= 1;
                            board->post[i].sn[j] = -1;
                            msg.assign("Delete successfully.");
                            break;
                        }
                    }
                    if (msg.size() > 0){
                        sem_post(&board->post[i].lock);
                        break;
                    }
                }
                sem_post(&board->post[i].lock);
            }
            if (msg.size() == 0)
                msg.assign("Post does not exist.");
        }

        else if (cmd_arg[0].compare("update-post") == 0){
            int sn = atoi(cmd_arg[1].c_str());
            // Check if the post exist or not
            for (int i = 1; i < 64; ++i){
                sem_wait(&board->post[i].lock);
                int num_post = board->num_post[i];
                if (num_post != 0){
                    for (int j = 0; j < num_post; ++j){
                        if (board->post[i].sn[j] == -1){
                            num_post += 1;
                            continue;
                        }
                        if (sn == board->post[i].sn[j]){
                            // Check if the post owner
                            std::string username = retName(std::to_string(key));
                            username.pop_back();
                            if (username.compare(board->post[i].author[j]) != 0){
                                msg.assign("Not the post owner.");
                                break;
                            }
                            // sn match
                            if (cmd_arg[2].compare("--title") == 0){
                                bzero(board->post[i].title[j], '\0');
                                strcpy(board->post[i].title[j], cmd_arg[3].c_str());
                            }
                            else if (cmd_arg[2].compare("--content") == 0){
                                bzero(board->post[i].content[j], '\0');
                                strcpy(board->post[i].content[j], cmd_arg[3].c_str());
                            }
                            msg.assign("Update successfully.");
                        }
                    }
                }
                if (msg.size() > 0){
                    sem_post(&board->post[i].lock);
                    break;
                }
                sem_post(&board->post[i].lock);
            }
            if (msg.size() == 0)
                msg.assign("Post does not exist.");
        }

        else if (cmd_arg[0].compare("comment") == 0){
            int sn = atoi(cmd_arg[1].c_str());
            // Check if the post exists or not
            for (int i = 1; i < 64; ++i){
                sem_wait(&board->post[i].lock);
                int num_post = board->num_post[i];
                if (num_post != 0){
                    for (int j = 0; j < num_post; ++j){
                        if (board->post[i].sn[j] == -1){
                            num_post += 1;
                            continue;
                        }
                        // sn match
                        if (sn == board->post[i].sn[j]){
                            std::string username = retName(std::to_string(key));
                            username.pop_back();
                            int num_comment = board->post[i].num_comment[j];
                            board->post[i].num_comment[j] += 1;
                            strcpy(board->post[i].user_comment[j].user[num_comment], username.c_str());
                            strcpy(board->post[i].user_comment[j].comment[num_comment], cmd_arg[2].c_str());
                            msg.assign("Comment successfully.");
                            break;
                        }
                    }
                }
                if (msg.size() > 0){
                    sem_post(&board->post[i].lock);
                    break;
                }
                sem_post(&board->post[i].lock);
            }
            if (msg.size() == 0)
                msg.assign("Post does not exist.");
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
        bzero(buf, BUF_SIZE);

        if (recvfrom(fd, buf, BUF_SIZE, 0, (sockaddr *)&clnt_addr, &clnt_len) < 0){
            fprintf(stderr, "recvfrom(): %s\n", std::strerror(errno));
            continue;
        }
        
        cmd.assign(buf);
        arg_count = splitCmd(cmd, cmd_arg);
        if (cmd_arg[0].compare("register") == 0){
            int rtn{insertData(cmd_arg[1], cmd_arg[2], cmd_arg[3])};
            if (rtn < 0)
                msg.assign("Username is already used.");
            else
                msg.assign("Register successfully.");
        }

        else if (cmd_arg[0].compare("whoami") == 0){
            msg.assign(retName(cmd_arg[1]));
        }

        // Send reply back to client
        sendto(fd, msg.c_str(), msg.size(), 0, (sockaddr *)&clnt_addr, clnt_len);

        msg.clear();
        cmd_arg.clear();
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
            "NAME VARCHAR(64) NOT NULL,"
            "FD INTERGER NOT NULL);";
    rc = sqlite3_exec(db, sql.c_str(), 0, 0, &err_msg);
    if (rc != SQLITE_OK){
        fprintf(stderr, "sqlite3_exec(): %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        exit(EXIT_FAILURE);
    }

    sql = "DROP TABLE IF EXISTS BOARD;"
            "CREATE TABLE BOARD("
            "ID INTEGER PRIMARY KEY AUTOINCREMENT,"
            "NAME VARCHAR(64) NOT NULL,"
            "MOD VARCHAR(64) NOT NULL);";
    rc = sqlite3_exec(db, sql.c_str(), 0, 0, &err_msg);
    if (rc != SQLITE_OK){
        fprintf(stderr, "sqlite3_exec(): %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        exit(EXIT_FAILURE);
    }

    sqlite3_close(db);
}

int tcpInitialise(sockaddr_in serv_addr){
    int fd{}, opt{1};
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        errorExit("[TCP]socket()");
    //if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    //    errorExit("[TCP]setsockopt()");
    if (bind(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        errorExit("[TCP]bind()");
    if (listen(fd, 10) < 0)
        errorExit("[TCP]listen()");

    return fd;
}

int udpInitialise(sockaddr_in serv_addr){
    int fd{};
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        errorExit("[UDP]socket()");
    if (bind(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        errorExit("[UDP]bind()");

    return fd;
}