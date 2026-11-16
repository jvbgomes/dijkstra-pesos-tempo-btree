CXX = g++
CXXFLAGS = -O3 -std=c++17 -Wall -Wextra

all: dijkstra_base dijkstra_td

dijkstra_base: src/dijkstra_base.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

# Sem bplus: variantes array, hash, map, vec
dijkstra_td: src/dijkstra_td.cpp src/traffic_types.hpp
	$(CXX) $(CXXFLAGS) -o $@ $<

# Com bplus: todas as variantes (binario padrao para experimentos)
dijkstra_td_bplus: src/dijkstra_td.cpp src/traffic_types.hpp src/bplus_store.hpp
	$(CXX) $(CXXFLAGS) -DUSE_BPLUS -o dijkstra_td $<

test_bplus: src/test_bplus.cpp src/bplus_store.hpp src/traffic_types.hpp
	$(CXX) -O2 -std=c++17 -DUSE_BPLUS -o $@ $<

clean:
	rm -f dijkstra_base dijkstra_td test_bplus
