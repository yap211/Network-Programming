PROG = server client
OBJ = server.o client.o funct.o acc_manage.o
DB = *.db

.PHONY : all clean

all : $(PROG) $(OBJ)

server : server.o funct.o chatroom_manage.o board_manage.o acc_manage.o funct.h chatroom_manage.h board_manage.h acc_manage.h
	g++ -o $@ server.o funct.o chatroom_manage.o board_manage.o acc_manage.o -lsqlite3 -lrt -pthread

client : client.o funct.o funct.h
	g++ -o $@ client.o funct.o -lrt -pthread

server.o : server.cpp funct.h 
	g++ -c $<

client.o : client.cpp funct.h
	g++ -c $<

funct.o : funct.cpp funct.h
	g++ -c $<

chatroom_manage.o : chatroom_manage.cpp chatroom_manage.h acc_manage.h
	g++ -c $<

board_manage.o : board_manage.cpp board_manage.h acc_manage.h
	g++ -c $<

acc_manage.o : acc_manage.cpp acc_manage.h
	g++ -c $<

clean:
	rm -f $(PROG) $(OBJ) $(DB)