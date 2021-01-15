#ifndef CHATROOM_MANAGE_H
#define CHATROOM_MANAGE_H

int checkStatus(std::string name);
int checkRoom(std::string name);
int insertRoom(std::string name, std::string port);
std::string listRoom();
int updateRoomStatus(std::string status, std::string name);

#endif