#include <string>
#include <sqlite3.h>
#include "acc_manage.h"
#include "chatroom_manage.h"

#define DB_NAME "np.db"

int checkStatus(std::string name){
    // Return chatroom status
    sqlite3 *db;
    int rc{}, rtn{};
    std::string sql, data;
    rc = sqlite3_open(DB_NAME, &db);

    sql = "SELECT STATUS FROM CHATROOM WHERE NAME='" + name + "';";
    rc = sqlite3_exec(db, sql.c_str(), retData, (void *)&data, NULL);
    data.pop_back();
    if (!data.compare("open"))
        rtn = 1;
    else
        rtn = 0;

    sqlite3_close(db);

    return rtn;
}

int checkRoom(std::string name){
    // If chatroom exist, return port number, else 0
    sqlite3 *db;
    int rc{}, rtn{};
    std::string sql, data;
    rc = sqlite3_open(DB_NAME, &db);

    sql = "SELECT PORT FROM CHATROOM WHERE NAME='" + name + "';";
    rc = sqlite3_exec(db, sql.c_str(), retData, (void *)&data, NULL);
    if (checkExist(data)){
        data.pop_back();
        rtn = atoi(data.c_str());
    }
    
    sqlite3_close(db);

    return rtn;
}

int insertRoom(std::string name, std::string port){
    // Insert username and port number into database
    sqlite3 *db;
    int rc{};
    std::string sql;
    rc = sqlite3_open(DB_NAME, &db);

    sql = "INSERT INTO CHATROOM (NAME,STATUS,PORT)"
            "VALUES ('" + name + "','open','" + port + "');";
    rc = sqlite3_exec(db, sql.c_str(), 0, 0, NULL);

    sqlite3_close(db);

    return 0;
}

std::string listRoom(){
    // Return a list of chatroom with name and status
    std::string sql, list;
    sqlite3 *db;
    int rc{};
    rc = sqlite3_open(DB_NAME, &db);

    sql = "SELECT NAME,STATUS FROM CHATROOM;";
    rc = sqlite3_exec(db, sql.c_str(), retData, (void *)&list, NULL);

    return list;
}

int updateRoomStatus(std::string status, std::string name){
    // 0: close, 1: open
    sqlite3 *db;
    int rc{};
    std::string sql;
    rc = sqlite3_open(DB_NAME, &db);

    sql = "UPDATE CHATROOM SET STATUS='" + status + "' WHERE NAME='" + name + "';";
    rc = sqlite3_exec(db, sql.c_str(), 0, 0, NULL);
    sqlite3_close(db);

    return 0;
}