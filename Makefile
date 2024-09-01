EXEC = panv0
CFLAGS = -std=c++11
CC = g++
SRC = src/simulator.cpp src/stack.cpp src/data.cpp src/instruction.cpp src/regfile.cpp src/panresult.cpp

OBJ = $(SRC:.cpp=.o)

$(EXEC): $(OBJ)
	$(CC) $(CFLAGS) -o $(EXEC) $(OBJ) -Iinclude

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@ -Iinclude

debug: CFLAGS += -g3
debug: $(EXEC)

clean:
	rm -f $(EXEC) $(OBJ)

.PHONY: debug clean
