#ifndef ACC_MANAGE_H
#define ACC_MANAGE_H

std::string retName(std::string key);
void deleteLog(std::string key);
int insertLog(std::string username);
int retData(void *data, int argc, char **argv, char **azColName);
int checkExist(std::string data);
std::string listUser();
int checkLogin(std::string username, std::string password);
int insertData(std::string username, std::string email, std::string password);

#endif
