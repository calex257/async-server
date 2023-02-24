CC = gcc
CFLAGS = -Wall -g
SRC_DIR = ./src
OUT_DIR = ./build
PARSER_DIR = ./dependencies/http-parser
SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ := $(patsubst $(SRC_DIR)/%.c,$(OUT_DIR)/%.o,$(SRC))

$(info $(OBJ))

all: build

build: server

server: $(OBJ) $(PARSER_DIR)/http_parser_g.o
	$(CC) $(CFLAGS) -o server $^ -laio

$(OUT_DIR)/list_utils.o: $(SRC_DIR)/list_utils.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(OUT_DIR)/path_utils.o: $(SRC_DIR)/path_utils.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(OUT_DIR)/server_utils.o: $(SRC_DIR)/server_utils.c $(PARSER_DIR)/http_parser_g.o
	$(CC) $(CFLAGS) -c $^ -o $@

$(OUT_DIR)/server.o: $(SRC_DIR)/server.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(PARSER_DIR)/http_parser_g.o: $(PARSER_DIR)/http_parser.c
	$(MAKE) -C $(PARSER_DIR)/ http_parser_g.o

.PHONY: clean
clean:
	rm -f server build/*.o
	$(MAKE) -C $(PARSER_DIR)/ clean