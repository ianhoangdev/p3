# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -std=c++11 -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26
LDFLAGS = -L../libWad -lWad -lfuse

# Targets
all: wadfs

wadfs: wadfs.cpp
	$(CXX) $(CXXFLAGS) -o wadfs wadfs.cpp $(LDFLAGS)

clean:
	rm -f wadfs *.o