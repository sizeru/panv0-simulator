EXEC = panv0
CC = gcc
SRC = simulator.cpp

$(EXEC):
	$(CC) -o $(EXEC) $(SRC)