EXEC = panv0
CFLAGS = -std=c++11
CC = g++
SRC = src/simulator.cpp src/stack.cpp src/data.cpp src/instruction.cpp

OBJ = $(SRC:.cpp=.o)

$(EXEC): $(OBJ)
	$(CC) $(CFLAGS) -o $(EXEC) $(OBJ) -I include

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@ -I include

debug: CFLAGS += -g3
debug: $(EXEC)

clean:
	rm -f $(EXEC) $(OBJ)

.PHONY: debug clean
