CC = g++
CFLAGS = -g -Wall
TARGET = ./


all:
	$(CC) $(CFLAGS) np_simple.cpp -o $(TARGET)np_simple
	$(CC) $(CFLAGS) np_single_proc.cpp -o $(TARGET)np_single_proc
	$(CC) $(CFLAGS) np_multi_proc.cpp -o $(TARGET)np_multi_proc
     
clean:
	rm $(TARGET)np_simple
	rm $(TARGET)np_single_proc
	rm $(TARGET)np_multi_proc
