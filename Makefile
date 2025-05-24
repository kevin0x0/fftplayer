ROOT_DIR = $(shell pwd)
INC_DIR = $(ROOT_DIR)/include
SRC_DIR = $(ROOT_DIR)/src
OBJ_DIR = $(ROOT_DIR)/obj
BIN_DIR = $(ROOT_DIR)/bin
BINARIES = $(BIN_DIR)/main
# OBJECTS defined here
include objects.mk

CC = gcc
DEBUG = -g
OPTIMIZE = -O3
CFLAGS = -I $(INC_DIR) $(DEBUG) $(OPTIMIZE) -Wall -Wextra
LIBS = -lm -lglfw

UNAME := $(shell uname -s 2>/dev/null || echo "Windows_NT")

ifeq ($(UNAME),Windows_NT)
	LINK_FLAGS = -L $(ROOT_DIR)/thirdparty
	LIBS += -lglfw3 -lgdi32 -lopengl32
else
	LIBS += -lasound
endif

$(BIN_DIR)/main: $(OBJECTS) | create_dir
	$(CC) -o $@ $^ $(CFLAGS) $(LINK_FLAGS) $(LIBS)

include deps.mk

.PHONY: create_dir clean

create_dir:
	mkdir -p $(OBJ_DIR) $(BIN_DIR)

clean:
	$(RM) $(OBJECTS) $(BINARIES)
