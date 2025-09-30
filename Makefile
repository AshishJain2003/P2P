CXX = g++
CXXFLAGS = -Wall -g -pthread
LDFLAGS = -lpthread

all: tracker client

tracker: tracker/main.cpp tracker/network.cpp tracker/commands.cpp tracker/state.cpp
	$(CXX) $(CXXFLAGS) $^ -o tracker.out $(LDFLAGS)

client: client/main.cpp client/network.cpp client/commands.cpp sha1.cpp
	$(CXX) $(CXXFLAGS) $^ -o client.out $(LDFLAGS)

clean:
	rm -f tracker.out client.out *.o