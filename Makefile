CXX = gcc
INPUT = ./server.c
OBJ = ./server.o
OUT = ./server
RM = rm -f
PORT = 6666

all: $(OUT)

run:
	$(OUT) $(PORT)

server: $(OBJ)
	$(CXX) -o $(OUT) $(OBJ)

server.o: $(INPUT)
	$(CXX) -c $(INPUT)

clean: $(OUT) $(OBJ)
	$(RM) $(OBJ)
	$(RM) $(OUT)