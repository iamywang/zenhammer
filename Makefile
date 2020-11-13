SRC_DIR := src
INC_DIR := include
OBJ_DIR := obj
BIN_DIR := bin
LOG_DIR := log

CXX = g++
CXXFLAGS = -Wall -std=c++11 -O0 -g -Wno-unused-variable -I$(INC_DIR) -I /usr/local/include -L /usr/local/lib
LIB_ASMJIT = -lasmjit

BIN_NAME := blacksmith
EXE := $(BIN_DIR)/$(BIN_NAME)
SRC := $(wildcard $(SRC_DIR)/*.cpp)
OBJ := $(SRC:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

all: $(EXE)

.PHONY: all

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(LIB_ASMJIT)

$(BIN_DIR) $(OBJ_DIR) $(LOG_DIR):
	mkdir -p $@

$(BIN_DIR)/$(BIN_NAME): $(OBJ) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIB_ASMJIT)

run: $(EXE)
	sudo $(EXE) 100

benchmark: $(EXE)
	@ts=$(shell date +"%Y%M%d_%H%M%S.log"); \
	echo "Writing log into $(shell pwd)/$(LOG_DIR)/$$ts"; \
	# sudo $(EXE) 100000 | tee $(LOG_DIR)/`date +"%Y%m%d_%H%M%S.log"`
	sudo $(EXE) | tee $(LOG_DIR)/`date +"%Y%m%d_%H%M%S.log"`

clean:
	@$(RM) -rv $(BIN_DIR) $(OBJ_DIR)

debug: $(EXE)
	sudo gdb -ex="set confirm off" $(EXE)

install_deps:
	# check if asmjit is installed: sudo ldconfig -p | grep asmjit
	git clone https://github.com/asmjit/asmjit && cd asmjit && cmake . && make && sudo make install && sudo ldconfig
