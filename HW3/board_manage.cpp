#include <ctime>
#include <iostream>
#include <string>
#include <sqlite3.h>
#include "acc_manage.h"
#include "board_manage.h"

#define DB_NAME "np.db"

std::string processContent(char *content){
    std::string new_content{content};
    std::string::size_type n{};
    while (1){
        n = new_content.find("<br>", n);
        if (n == std::string::npos)
            break;
        new_content.replace(n, 4, "\n");
        n += 1;
    }
    
    return new_content;
}

int checkBoard(std::string name){
    // If board exist, return board id, else -1;
    sqlite3 *db;
    int rc{}, rtn{};
    std::string sql, data;
    rc = sqlite3_open(DB_NAME, &db);

    sql = "SELECT ID FROM BOARD WHERE NAME='" + name + "';";
    rc = sqlite3_exec(db, sql.c_str(), retData, (void *)&data, NULL);
    if (checkExist(data)){
        data.pop_back();
        rtn = atoi(data.c_str());
    }
    else {
        rtn = -1;
    }

    sqlite3_close(db);

    return rtn;
}

int insertBoard(std::string name, std::string mod){
    // Insert new board into table. Return -1 if board name duplicated; else 0
    sqlite3 *db;
    int rc{};
    std::string sql, data;
    rc = sqlite3_open(DB_NAME, &db);

    sql = "SELECT * FROM BOARD WHERE NAME='" + name + "';";
    rc = sqlite3_exec(db, sql.c_str(), retData, (void *)&data, NULL);
    if (checkExist(data)){
        sqlite3_close(db);
        return -1;
    } 
    else {
        sql = "INSERT INTO BOARD (NAME,MOD)"
                "VALUES ('" + name + "','" + mod + "');";
        rc = sqlite3_exec(db, sql.c_str(), 0, 0, NULL);
    }
    sqlite3_close(db);

    return 0;
}

std::string listBoard(){
    // Return a list of board with id, board name and moderator name
    std::string list;
    sqlite3 *db;
    int rc{};
    std::string sql;
    rc = sqlite3_open(DB_NAME, &db);

    sql = "SELECT * FROM BOARD;";
    rc = sqlite3_exec(db, sql.c_str(), retData, (void *)&list, NULL);
    sqlite3_close(db);

    return list;
}