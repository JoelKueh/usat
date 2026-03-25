
OPT_LEVEL=-O2

OUT_DIR=out
OBJ_DIR=obj
BIN_DIR=bin
SRC_DIR=src

CFLAGS=$(INCLUDE_DIRS) $(LD_FLAGS) $(OPT_LEVEL) -std=c++11

.PHONY: default clean debug
default: $(BIN_DIR)/usat

debug: OPT_LEVEL= 
debug: CFLAGS += -g
debug: default

USAT_SRCS=src/cnf.cpp src/usat.cpp
USAT_OBJS=$(USAT_SRCS:src/%.cpp=obj/%.o)
USAT_DEPS=$(USAT_OBJS:%.o=%.d)
-include $(USAT_DEPS)

obj/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	g++ -MMD -MP -MF $(@:.o=.d) -c -o $@ $< $(cflags) 

$(BIN_DIR)/usat: obj/usat.o $(USAT_OBJS)
	@mkdir -p $(dir $@)
	@mkdir -p $(OUT_DIR)
	g++ -o $@ $(USAT_OBJS) $(CFLAGS)

clean:
	rm -rf ./$(OBJ_DIR)/* ./$(BIN_DIR)/*
