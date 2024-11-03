ROOT_DIR = $(shell pwd)
INC_DIR = $(ROOT_DIR)/include
SRC_DIR = $(ROOT_DIR)/src
OBJ_DIR = $(ROOT_DIR)/obj
BIN_DIR = $(ROOT_DIR)/bin
THIRDPARTY_DIR= $(ROOT_DIR)/thirdparty
BINARIES = $(BIN_DIR)/main
# OBJECTS defined here
include objects.mk

CC = gcc
DEBUG = -g
OPTIMIZE = -O3
CFLAGS = -I $(INC_DIR) $(DEBUG) $(OPTIMIZE) -Wall -Wextra

$(BIN_DIR)/main: $(OBJECTS) | create_dir
	$(CC) -o $@ $^ $(CFLAGS) -L $(THIRDPARTY_DIR) -lglfw3 -lopengl32 -lgdi32 -lwinmm

include deps.mk

.PHONY: create_dir clean

create_dir:
	mkdir -p $(OBJ_DIR) $(BIN_DIR)

clean:
	$(RM) $(OBJECTS) $(BINARIES)
