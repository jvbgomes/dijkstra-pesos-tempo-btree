CXX = g++
CXXFLAGS = -O3 -std=c++17 -Wall -Wextra

all: dijkstra_base dijkstra_td

dijkstra_base: src/dijkstra_base.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

dijkstra_td: src/dijkstra_td.cpp src/traffic_types.hpp src/bplus_store.hpp
	$(CXX) $(CXXFLAGS) -o $@ $<

test_bplus: src/test_bplus.cpp src/bplus_store.hpp src/traffic_types.hpp
	$(CXX) -O2 -std=c++17 -o $@ $<

clean:
	rm -f dijkstra_base dijkstra_td test_bplus
