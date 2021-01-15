#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>
#include <sqlite3.h>
#include <vector>
#include "acc_manage.h"

#define DB_NAME "np.db"

int checkLogin(std::string username, std::string password){
    // Return 0 if login failed, 1 if success
    sqlite3 *db;
    int rc{}, rtn{};
    std::string sql, data;
    rc = sqlite3_open(DB_NAME, &db);

    sql = "SELECT * FROM USERINFO WHERE NAME='" + username + "' AND PASSWORD='" + password + "';";
    rc = sqlite3_exec(db, sql.c_str(), retData, (void *)&data, NULL);
    rtn = checkExist(data);
    
    return rtn;
}

int insertData(std::string username, std::string email, std::string password){
    // Insert new user into table. Return -1 if username already exists; else 0
    sqlite3 *db;
    int rc{};
    std::string sql, data;
    rc = sqlite3_open(DB_NAME, &db);

    sql = "SELECT * FROM USERINFO WHERE NAME='" + username + "';";
    rc = sqlite3_exec(db, sql.c_str(), retData, (void *)&data, NULL);
    if (checkExist(data)){
        sqlite3_close(db);
        return -1;
    }
    else {
        sql = "INSERT INTO USERINFO (NAME,PASSWORD,EMAIL)"
		        "VALUES ('" + username + "','" + password + "','" + email + "');";
        rc = sqlite3_exec(db, sql.c_str(), 0, 0, NULL);
    }
    sqlite3_close(db);

    return 0;
}

std::string listUser(){
    // Return a list of user with name and email
    std::string list;
    sqlite3 *db;
    int rc{};
    std::string sql;
    rc = sqlite3_open(DB_NAME, &db);

    sql = "SELECT NAME,EMAIL FROM USERINFO;";
    rc = sqlite3_exec(db, sql.c_str(), retData, (void *)&list, NULL);
    sqlite3_close(db);

    return list;
}

int retData(void *data, int argc, char **argv, char **azColName){
    std::string *list = (std::string *)data;

    for (int i = 0; i < argc; ++i){
        (*list).append(argv[i]);
            if (i != argc - 1)
                (*list).append("\t");
    }
    (*list).append("\n");

    return 0;
}

int checkExist(std::string data){
    // return 1 if exist; else, 0
	std::size_t found = data.find_first_not_of("\n");
    if (found == std::string::npos)
        return 0;

	return 1;
}

int insertLog(std::string username, int fd){
    // Check if random generated number is duplicated, then insert the number and username into table
    int rng = std::rand();

    sqlite3 *db;
    int rc{};
    std::string sql, data;
    rc = sqlite3_open(DB_NAME, &db);

    sql = "SELECT * FROM LOG WHERE ID='" + std::to_string(rng) + "';";
    rc = sqlite3_exec(db, sql.c_str(), retData, (void *)&data, NULL);
    while (checkExist(data)){
        data.clear();
        rng = std::rand();
        rc = sqlite3_exec(db, sql.c_str(), retData, (void *)&data, NULL);
    }
    sql = "INSERT INTO LOG (ID,NAME,FD)"
		   "VALUES ('" + std::to_string(rng) + "','" + username + "','" + std::to_string(fd) + "');";
    rc = sqlite3_exec(db, sql.c_str(), 0, 0, NULL);
    
    
    sqlite3_close(db);

    return rng;
}

void deleteLog(std::string key){
    sqlite3 *db;
    int rc{};
    std::string sql, data;
    rc = sqlite3_open(DB_NAME, &db);

    sql = "DELETE FROM LOG WHERE ID='" + key + "';";
    rc = sqlite3_exec(db, sql.c_str(), retData, (void *)&data, NULL);

    sqlite3_close(db);
}

std::string retName(std::string key){
    std::string username;

    sqlite3 *db;
    int rc{};
    std::string sql;
    rc = sqlite3_open(DB_NAME, &db);

    sql = "SELECT NAME FROM LOG WHERE ID=" + key + ";";
    rc = sqlite3_exec(db, sql.c_str(), retData, (void *)&username, NULL);

    sqlite3_close(db);

    username.pop_back();

    return username;
}
