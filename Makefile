CXX = g++
CXXFLAGS = -Wall -g
LDFLAGS = -lpthread

all: tracker client

tracker: tracker/main.cpp tracker/network.cpp tracker/commands.cpp tracker/state.cpp
	$(CXX) $(CXXFLAGS) $^ -o tracker.out $(LDFLAGS)

client: client/main.cpp client/network.cpp client/commands.cpp
	$(CXX) $(CXXFLAGS) $^ -o client.out

clean:
	rm -f tracker.out client.out