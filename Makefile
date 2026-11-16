CXX = g++
CXXFLAGS = -O3 -std=c++17 -Wall -Wextra

all: dijkstra_base dijkstra_td

dijkstra_base: src/dijkstra_base.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

dijkstra_td: src/dijkstra_td.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f dijkstra_base dijkstra_td
