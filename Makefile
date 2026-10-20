# ArtiTrack build.  Usage:  make          (builds ./artitrack)
#                           make test     (builds and runs the harness + menu tests)
#                           make clean
# On Windows use MinGW's  mingw32-make,  or run ./build.sh in Git Bash.
CC       ?= gcc
CXX      ?= g++
CFLAGS   = -std=c11 -Wall -Wextra -O2
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

# pure C:  m3_core.c (hash table, merge sort, graph)   m4.c (linked list, stack, heap)
# C++:     everything else
OBJS = main.o m0.o m1.o m2.o m3.o m3_core.o m4.o bridge.o
LIB_OBJS = m0.o m1.o m2.o m3.o m3_core.o m4.o bridge.o     # everything except main.o

artitrack: $(OBJS)
	$(CXX) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

m3_harness: tests/m3_harness.cpp $(LIB_OBJS)
	$(CXX) $(CXXFLAGS) -I. tests/m3_harness.cpp $(LIB_OBJS) -o $@

test: artitrack m3_harness
	bash tests/run_tests.sh

clean:
	rm -f *.o artitrack m3_harness

# header dependencies
main.o: m0.h m1.h m2.h m3.h m4.h bridge.h
m0.o: m0.h
m1.o: m1.h m2.h
m2.o: m2.h
m3.o: m3.h m3_core.h m2.h m0.h
m3_core.o: m3_core.h
m4.o: m4.h
bridge.o: bridge.h m1.h m2.h m4.h
