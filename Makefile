CXX = g++

LIBS = 
WARN = -Wall
INCL = -I. -Iworker
#OPT	 = -O3
OPT	= -g3

SRCS = $(wildcard worker/*.cpp)
OBJS = $(addprefix objs/, $(subst /,_,$(SRCS:.cpp=.o)))

CXXFLAGS = -std=gnu++11 $(WARN) $(OPT) $(INCL)

default: dirs bin/worker

dirs:
	/bin/mkdir -p bin objs
	
clean:
	/bin/rm -rf bin/worker objs/*.o

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
