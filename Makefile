EXEC = panv0
CFLAGS =
CC = g++
SRC = simulator.cpp

$(EXEC): $(SRC)
	$(CC) $(CFLAGS) -o $(EXEC) $(SRC)

debug: CFLAGS += -g3
debug: $(EXEC)

clean:
	rm $(EXEC)

.PHONY: debug clean