CC = g++
CFLAGS = -Wall -g

server: main.o WebServer.o Utils.o Http.o
	$(CC) $(CFLAGS) *.o -lpthread -o server

main.o: main.cpp	
	$(CC) $(CFLAGS) -c main.cpp

WebServer.o: ./Webserver/webserver.cpp
	$(CC) $(CFLAGS) -c ./Webserver/webserver.cpp

Utils.o: ./Utils/utils.cpp
	$(CC) $(CFLAGS) -c ./Utils/utils.cpp

Http.o: ./Http/httpconn.cpp
	$(CC) $(CFLAGS) -c ./Http/httpconn.cpp

#Timer.o: ./Timer/Timer.cpp
#	$(CC) $(CFLAGS) -c ./Timer/Timer.cpp

clean:
	rm *.o -f  