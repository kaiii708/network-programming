CC = g++
CFLAGS = -g -Wall -lpthread
TARGET = ./

all:
	$(CC) socks_server.cpp -o $(TARGET)socks_server $(CFLAGS)
	$(CC) console.cgi.cpp -o $(TARGET)hw4.cgi $(CFLAGS)

clean:
	rm $(TARGET)socks_server
	rm $(TARGET)hw4.cgi
	rm $(TARGET)http_server
