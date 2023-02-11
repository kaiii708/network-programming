CC = g++
CFLAGS = -g -Wall -lpthread
TARGET = ./

all:
	$(CC) http_server.cpp -o $(TARGET)http_server $(CFLAGS)
	$(CC) console.cgi.cpp -o $(TARGET)console.cgi $(CFLAGS)
	$(CC) cgi_server.cpp -o $(TARGET)cgi_server $(CFLAGS)

part1:
	$(CC) http_server.cpp -o $(TARGET)http_server $(CFLAGS)
	$(CC) console.cgi.cpp -o $(TARGET)console.cgi $(CFLAGS)
part2:
	g++ cgi_server.cpp -o $(TARGET)cgi_server.exe -lws2_32 -lwsock32 -std=c++14

clean:
	rm $(TARGET)http_server
	rm $(TARGET)console.cgi
