
OPT_LEVEL=-O2

OUT_DIR=out
OBJ_DIR=obj
BIN_DIR=bin
SRC_DIR=src

OPT_LEVEL=-O3
INC_DIRS=-I./inc
CFLAGS=$(INC_DIRS) $(OPT_LEVEL) -std=c++17

.PHONY: default clean debug release test test-debug
default: $(BIN_DIR)/usat
release: $(BIN_DIR)/usat
test: $(BIN_DIR)/test

test-debug: OPT_LEVEL= 
test-debug: CFLAGS += -g
test-debug: test

debug: OPT_LEVEL= 
debug: CFLAGS += -g
debug: default

USAT_SRCS=src/cnf.cpp src/usat.cpp
USAT_OBJS=$(USAT_SRCS:src/%.cpp=obj/%.o)
USAT_DEPS=$(USAT_OBJS:%.o=%.d)
-include $(USAT_DEPS)

TEST_SRCS=src/cnf.cpp src/test.cpp
TEST_OBJS=$(TEST_SRCS:src/%.cpp=obj/%.o)
TEST_DEPS=$(TEST_OBJS:%.o=%.d)
-include $(TEST_DEPS)

obj/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	g++ -MMD -MP -MF $(@:.o=.d) -c -o $@ $< $(CFLAGS) 

$(BIN_DIR)/usat: $(USAT_OBJS)
	@mkdir -p $(dir $@)
	@mkdir -p $(OUT_DIR)
	g++ -o $@ $(USAT_OBJS) $(CFLAGS)

$(BIN_DIR)/test: $(TEST_OBJS)
	@mkdir -p $(dir $@)
	@mkdir -p $(OUT_DIR)
	g++ -o $@ $(TEST_OBJS) $(CFLAGS)

clean:
	rm -rf ./$(OBJ_DIR)/* ./$(BIN_DIR)/*
