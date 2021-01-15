#ifndef BOARD_MANAGE_H
#define BOARD_MANAGE_H

std::string processContent(char *content);
int checkBoard(std::string board_name);
int insertBoard(std::string name, std::string mod);
std::string listBoard();

#endif