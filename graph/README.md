# Graph module

This module provides:

- `graph/Graph.h`: a weighted directed graph keyed by `std::string`
- `graph/MetisGraph.h`: a METIS adapter (CSR conversion + partitioning)

It is used by the partitioned storage layer to build an access graph and compute partitions.

## `Graph`

`Graph` stores:

- **Vertex weights**: access counts per key
- **Edge weights**: co-access counts between keys

Implementation notes:

- Uses `ankerl::unordered_dense::map` for vertices and adjacency lists.
- Vertices and edges are created on first increment.

### Core API

- `increment_vertex_weight(vertex)`
- `increment_edge_weight(source, destination)`
- `get_vertex_weight(vertex)`
- `get_edge_weight(source, destination)`
- `get_vertex_count()`, `get_edge_count()`
- `clear()`

## `MetisGraph`

`MetisGraph` converts a `Graph` into METIS CSR data structures and runs METIS partitioning:

- `prepare_from_graph(graph)`: builds CSR arrays and internal index mappings
- `partition(num_partitions)`: runs METIS (recursive bisection for small `nparts`, k-way otherwise)

## Build targets

From the repository root:

```bash
./build.sh
```

From `build/`:

```bash
cd build
./test_graph
./example_graph
```

If METIS is available, additional targets are built:

```bash
cd build
./test_metis_graph
./example_metis_graph
```

