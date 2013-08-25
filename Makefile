HAVE_CLANG = $(shell which clang)

ifeq ($(HAVE_CLANG),)
    CXX = g++ -std=gnu++11
else
    CXX = clang++ -std=c++11
endif

ARCH = $(shell uname)

LIBS = -lboost_regex -lboost_program_options
WARN = -Wall
INCL = -I. -Iworker
#OPT	 = -O3
OPT	= -g3

ifeq ($(ARCH),Darwin)
    BOOST_DIR = $(shell brew --prefix boost)
    LIBS += -L$(BOOST_DIR)/lib
    INCL += -I$(BOOST_DIR)/include
endif

SRCS = $(wildcard worker/*.cpp)
OBJS = $(addprefix objs/, $(subst /,_,$(SRCS:.cpp=.o)))

CXXFLAGS = $(WARN) $(OPT) $(INCL)

default: dirs bin/worker

dirs:
	/bin/mkdir -p bin objs
	
clean:
	/bin/rm -rf bin/worker objs/*
	
rebuild: clean default

bin/worker: objs/main.o objs/libworker.a
	@echo "Linking $@"
	@$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)
	
objs/main.o : main.cpp
	@echo "Building $@"
	@$(CXX) $(CXXFLAGS) -o $@ -c $<
	
objs/worker_%.o: worker/%.cpp
	@echo "Building $@"
	@$(CXX) $(CXXFLAGS) -o $@ -c $< 

objs/libworker.a: $(OBJS)
	@echo "Building library $@"
	@ar rcs $@ $^
