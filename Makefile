EXEC = panv0
CC = g++
SRC = simulator.cpp

$(EXEC): $(SRC)
	$(CC) -o $(EXEC) $(SRC)