PROG = server client
OBJ = server.o client.o funct.o acc_manage.o
DB = *.db

.PHONY : all clean

all : $(PROG) $(OBJ)

server : server.o funct.o acc_manage.o funct.h acc_manage.h
	g++ -o $@ server.o funct.o acc_manage.o -lsqlite3 -lrt

client : client.o funct.o funct.h
	g++ -o $@ client.o funct.o

server.o : server.cpp funct.h
	g++ -c $<

client.o : client.cpp funct.h
	g++ -c $<

funct.o : funct.cpp funct.h
	g++ -c $<

acc_manage.o : acc_manage.cpp acc_manage.h
	g++ -c $<

clean:
	rm -f $(PROG) $(OBJ) $(DB)
