HAVE_CLANG = $(shell which clang)

ifeq ($(HAVE_CLANG),)
	CXX = g++ -std=gnu++11
else
	CXX = clang++ -std=c++11
endif

ARCH = $(shell uname)

LIBS = -lboost_regex -lboost_program_options -lboost_iostreams
WARN = -Wall
INCL = -I. -Iworker
OPT	 = -O3
#OPT	= -g3

ifeq ($(ARCH),Darwin)
	BOOST_DIR = $(shell brew --prefix boost)
	LIBS += -L$(BOOST_DIR)/lib
	INCL += -I$(BOOST_DIR)/include
endif

SRCS = $(wildcard worker/*.cpp)
OBJS = $(addprefix objs/, $(subst /,_,$(SRCS:.cpp=.o)))

CXXFLAGS = $(WARN) $(OPT) $(INCL)

define mkdir
mkdir -p $@
endef

define compile
@echo "Building $@"
@$(CXX) $(CXXFLAGS) -o $@ -c $<
endef

.PHONY: default lib clean rebuild

default: lib bin bin/worker

lib: objs objs/libworker.a

clean:
	rm -rf bin objs

rebuild: clean default

objs:
	$(mkdir)

bin:
	$(mkdir)

bin/worker: objs/main.o objs/libworker.a
	@echo "Linking $@"
	@$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

objs/main.o : main.cpp
	$(compile)

objs/worker_%.o: worker/%.cpp
	$(compile)

objs/libworker.a: $(OBJS)
	@echo "Building library $@"
	@ar rcs $@ $^
