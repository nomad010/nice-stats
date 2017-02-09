rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))

DIRS := ${shell find src/ -type d -print}
OBJ_DIRS = $(patsubst src/%, obj/%, $(DIRS))
SRC = $(call rwildcard,,*.cpp)

OBJS = $(patsubst %.cpp, %.o, $(SRC))
RELEASE_OBJS = $(patsubst src/%.cpp, obj/release/%.o, $(SRC))
DEBUG_OBJS = $(patsubst src/%.cpp, obj/debug/%.o, $(SRC))

RELEASE_DEPS = $(patsubst src/%.cpp, dep/release/%.o, $(SRC))
DEBUG_DEPS = $(patsubst src/%.cpp, dep/debug/%.o, $(SRC))

TESTS = $(patsubst src/test_%.cpp, %, $(wildcard src/test_*.cpp))
TEST_RESULTS = $(patsubst %, %_perform, $(TESTS))

EXECS = $(patsubst src/main_%.cpp, %, $(wildcard src/main_*.cpp))
RELEASE_EXECS = $(EXECS)
DEBUG_EXECS = $(patsubst %, %_debug, $(EXECS))

build: debug release

-include $(DEBUG_DEPS) $(RELEASE_DEPS)

CXX=clang++-3.8
INCLUDES=-I ./src/
CXXFLAGS=-Wall -Wextra -Wno-unused -Wno-unused-parameter -std=c++14 $(INCLUDES)
CXXLIBS=-lm -lpthread -lncurses

.PRECIOUS: $(DEBUG_DEPS) $(RELEASE_DEPS)

.PHONY: clean

debug: obj/debug $(DEBUG_EXECS)
	
release: obj/release $(RELEASE_EXECS)

test: tests $(TEST_RESULTS) 

tests:
	@mkdir -p tests
	
obj/debug:
	@mkdir -p obj/debug

$(DEBUG_EXECS): $(DEBUG_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ obj/debug/main_$(subst _debug,,$@).o $(filter-out obj/debug/test_%.o, $(filter-out obj/debug/main_%.o, $(DEBUG_OBJS))) $(CXXLIBS)

obj/debug/%.o: src/%.cpp
	@mkdir -p `dirname $@`
	$(CXX) -g $(CXXFLAGS) -c -o $@ $<
	@mkdir -p `dirname $(subst obj/debug/,dep/debug/,$@)`
	@$(CXX) -MM $(CXXFLAGS) $< > $(subst obj/debug/,dep/debug/,$@)
	@sed -i -e 's|.*:|$@:|' $(subst obj/debug/,dep/debug/,$@)
	@sed -i -e 's|.d:|.o:|' $(subst obj/debug/,dep/debug/,$@)
	@sed -i -e 's|^dep/|obj/|' $(subst obj/debug/,dep/debug/,$@)

obj/release:
	@mkdir -p obj/release

$(RELEASE_EXECS): $(RELEASE_OBJS)
	$(CXX) $(CXXFLAGS) -flto -o $@ obj/release/main_$@.o $(filter-out obj/release/test_%.o, $(filter-out obj/release/main_%.o, $(RELEASE_OBJS))) $(CXXLIBS)

obj/release/%.o: src/%.cpp
	@mkdir -p `dirname $@`
	$(CXX) -O3 $(CXXFLAGS) -c -o $@ $<
	@mkdir -p `dirname $(subst obj/release/,dep/release/,$@)`
	@$(CXX) -MM $(CXXFLAGS) $< > $(subst obj/release/,dep/release/,$@)
	@sed -i -e 's|.*:|$@:|' $(subst obj/release/,dep/release/,$@)
	@sed -i -e 's|.d:|.o:|' $(subst obj/release/,dep/release/,$@)
	@sed -i -e 's|^dep/|obj/|' $(subst obj/release/,dep/release/,$@)

$(TESTS): $(RELEASE_OBJS)
	$(CXX) $(CXXFLAGS) -o tests/$@ obj/release/test_$@.o $(filter-out obj/release/test_%.o, $(filter-out obj/release/main_%.o, $(RELEASE_OBJS))) $(CXXLIBS)

$(TEST_RESULTS): $(TESTS)
	@(./tests/$(subst _perform,,$@) && (echo "\033[92m$(subst _perform,,$@)\033[0m") || (echo "\033[91m$(subst _perform,,$@)\033[0m";))

clean:
	rm -f $(RELEASE_EXECS) $(RELEASE_OBJS) $(DEBUG_EXECS) $(DEBUG_OBJS) $(TESTS) $(DEBUG_DEPS) $(RELEASE_DEPS)
	rm -rf dep
	rm -rf obj
	rm -rf tests

check-syntax:
	$(CXX) $(CXXFLAGS) -Wextra -Wno-sign-compare -fsyntax-only $(CHK_SOURCES)
