# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -std=c++11

# Targets
all: libWad.a

libWad.a: Wad.o
	ar rcs libWad.a Wad.o

Wad.o: Wad.cpp Wad.h
	$(CXX) $(CXXFLAGS) -c Wad.cpp -o Wad.o

clean:
	rm -f *.o *.a