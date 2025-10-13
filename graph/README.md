# Graph

A high-performance weighted directed graph implementation in C++.

## Overview

The `Graph` class provides an efficient implementation of a weighted directed graph using adjacency list representation with `std::unordered_map`. Both vertices and edges have associated integer weights that can be incremented.

## Features

- **Weighted Vertices**: Each vertex has an associated integer weight
- **Weighted Edges**: Each directed edge has an associated integer weight
- **Efficient Operations**: O(1) average case for vertex and edge operations using hash maps
- **Automatic Creation**: Vertices and edges are created automatically when incremented
- **Simple API**: Easy-to-use interface for common graph operations

## Data Structure

- **Vertices**: `std::unordered_map<std::string, int>` - Maps vertex names to weights
- **Edges**: `std::unordered_map<std::string, std::unordered_map<std::string, int>>` - Maps source vertices to destination vertices and their weights (adjacency list)

## API Reference

### Core Operations

- `int increment_vertex_weight(const std::string& vertex)` - Increments vertex weight by 1 (creates with weight 1 if new)
- `int increment_edge_weight(const std::string& source, const std::string& destination)` - Increments edge weight by 1 (creates with weight 1 if new)

### Query Operations

- `int get_vertex_weight(const std::string& vertex) const` - Returns vertex weight (0 if not exists)
- `int get_edge_weight(const std::string& source, const std::string& destination) const` - Returns edge weight (0 if not exists)
- `bool has_vertex(const std::string& vertex) const` - Checks if vertex exists
- `bool has_edge(const std::string& source, const std::string& destination) const` - Checks if edge exists
- `size_t get_vertex_count() const` - Returns number of vertices
- `size_t get_edge_count() const` - Returns total number of edges

### Data Access

- `const std::unordered_map<std::string, int>& get_vertices() const` - Returns reference to vertices map
- `const std::unordered_map<std::string, std::unordered_map<std::string, int>>& get_edges() const` - Returns reference to edges map

### Utility

- `void clear()` - Removes all vertices and edges

## Usage Example

```cpp
#include "Graph.h"
#include <iostream>

int main() {
    Graph graph;

    // Increment vertex weights
    graph.increment_vertex_weight("A");  // Returns 1 (created with weight 1)
    graph.increment_vertex_weight("A");  // Returns 2 (incremented)
    graph.increment_vertex_weight("B");  // Returns 1 (created with weight 1)

    // Increment edge weights
    graph.increment_edge_weight("A", "B");  // Returns 1 (created with weight 1)
    graph.increment_edge_weight("A", "B");  // Returns 2 (incremented)
    graph.increment_edge_weight("A", "C");  // Returns 1 (created with weight 1)

    // Query the graph
    std::cout << "Vertex A weight: " << graph.get_vertex_weight("A") << std::endl;
    std::cout << "Edge A->B weight: " << graph.get_edge_weight("A", "B") << std::endl;
    std::cout << "Has edge B->A: " << graph.has_edge("B", "A") << std::endl;

    return 0;
}
```

## Building

To compile the example:

```bash
g++ -std=c++17 -O3 -o example_graph example_graph.cpp
./example_graph
```

To compile and run the tests:

```bash
g++ -std=c++17 -O3 -o test_graph test_graph.cpp
./test_graph
```

## Performance

The Graph class is optimized for performance:

- **Average Case**: O(1) for vertex and edge insertion/lookup operations
- **Hash-based**: Uses `std::unordered_map` for fast access
- **No Unnecessary Copies**: Uses const references where appropriate
- **Efficient Memory**: Only stores existing vertices and edges

Performance test results (on typical hardware):
- 100,000 vertex operations: ~10-30ms
- 100,000 edge operations: ~20-50ms

## Design Decisions

1. **Directed Graph**: The graph is directed by default. If an undirected graph is needed, simply add both directions when adding an edge.

2. **String Keys**: Vertices use string identifiers for maximum flexibility and readability.

3. **Integer Weights**: Weights are integers for simplicity and performance. Can be easily extended to other numeric types if needed.

4. **Automatic Creation**: Vertices and edges are automatically created when incremented, simplifying the API and reducing boilerplate code.

5. **Header-Only**: The entire implementation is in the header file for easy integration and potential inlining optimizations.

