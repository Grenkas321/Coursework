#include "GridDAGGenerator.h"

namespace scheduling_problem::additionals
{
    /**
     * @brief Construct a grid-based DAG generator.
     *
     * Initializes distributions for the number of columns and vertex weights,
     * stores the chosen grid topology, RNG seed, and name prefix.
     *
     * @param nb_columns  Inclusive range for the number of columns.
     * @param grid_type   Grid topology (TRIANGLE or RECTANGLE).
     * @param weights     Inclusive range for vertex weights.
     * @param n_samples   Total number of graphs to generate.
     * @param batch_size  Number of graphs returned per nextBatch() call.
     * @param seed        RNG seed.
     * @param prefix      Name prefix for generated graphs.
     */
    GridDAGGenerator::GridDAGGenerator(const std::pair<size_t, size_t> &nb_columns,
                                       const GridType &grid_type,
                                       const std::pair<size_t, size_t> &weights,
                                       unsigned n_samples,
                                       unsigned batch_size,
                                       unsigned seed,
                                       std::string prefix)
        : DAGPool(n_samples, batch_size), nb_columns_(nb_columns.first, nb_columns.second), grid_type_(grid_type), weights_(weights.first, weights.second), rng_(seed), prefix_(prefix)
    {
    }

    /**
     * @brief Generate the next batch of grid DAGs.
     *
     * Samples a single grid width and a corresponding vector of vertex weights,
     * then emits up to @p batch_size_ graphs with names prefix_ + index.
     *
     * @return Batch of generated graphs with assigned batch id.
     */
    DAGPool::Batch GridDAGGenerator::nextBatch()
    {
        unsigned sample(0);
        Batch graphs(batch_id_++);
        auto nb_columns = nb_columns_(rng_);
        size_t nb_vertex = ((nb_columns + 1) * nb_columns) / 2;
        std::vector<weight_t> weights(nb_vertex);
        std::generate(weights.begin(), weights.end(), [&]()
                      { return weights_(rng_); });
        while (current_sample_ < n_samples_ && sample < batch_size_)
        {
            auto graph_name = prefix_ + std::to_string(current_sample_);
            graphs.push_back(make(nb_columns, grid_type_, weights, graph_name));
            current_sample_++;
            sample++;
        }
        return graphs;
    }

    /**
     * @brief Build a grid DAG from fixed weights.
     *
     * Dispatches to the appropriate topology-specific constructor.
     *
     * @param nb_columns Number of columns in the grid.
     * @param grid_type  Grid topology selector.
     * @param weights    Per-vertex weights (size must match implied vertex count).
     * @param name       Graph name.
     * @return Generated DAG.
     */
    Graph GridDAGGenerator::make(size_t nb_columns,
                                 const GridType &grid_type,
                                 const std::vector<weight_t> &weights,
                                 const std::string &name)
    {
        if (grid_type == GridType::TRIANGLE)
        {
            return makeTriangleGridDAG(nb_columns, weights, name);
        }
        else
        {
            return makeRectangleGridDAG(nb_columns, weights, name);
        }
    }

    /**
     * @brief Build a grid DAG, sampling vertex weights from a range using an external RNG.
     *
     * Computes the vertex count implied by @p nb_columns and @p grid_type,
     * samples integer weights uniformly in the given range, then forwards to
     * the fixed-weights overload.
     *
     * @param nb_columns   Number of columns in the grid.
     * @param grid_type    Grid topology selector.
     * @param weights_minmax Inclusive range for vertex weights.
     * @param rng          RNG instance.
     * @param name         Graph name.
     * @return Generated DAG.
     */
    Graph GridDAGGenerator::make(size_t nb_columns,
                                 const GridType &grid_type,
                                 const std::pair<size_t, size_t> &weights_minmax,
                                 std::mt19937 &rng,
                                 const std::string &name)
    {
        size_t nb_vertex = 0;
        switch (grid_type)
        {
        case GridType::TRIANGLE:
            nb_vertex = ((nb_columns + 1) * nb_columns) / 2;
            break;
        case GridType::RECTANGLE:
            nb_vertex = nb_columns * nb_columns;
            break;
        default:
            throw std::runtime_error("Unknown GridType");
        }
        std::vector<weight_t> weights(nb_vertex);
        uniform_weight_t weights_dist(weights_minmax.first, weights_minmax.second);
        std::generate(weights.begin(), weights.end(), [&]()
                      { return weights_dist(rng); });
        return make(nb_columns, grid_type, weights, name);
    }

    /**
     * @brief Build a grid DAG, sampling vertex weights from a range using a local RNG.
     *
     * Convenience overload that constructs an RNG from @p seed and forwards to
     * the RNG-based overload.
     *
     * @param nb_columns   Number of columns in the grid.
     * @param grid_type    Grid topology selector.
     * @param weights_minmax Inclusive range for vertex weights.
     * @param name         Graph name.
     * @param seed         RNG seed.
     * @return Generated DAG.
     */
    Graph GridDAGGenerator::make(size_t nb_columns,
                                 const GridType &grid_type,
                                 const std::pair<size_t, size_t> &weights_minmax,
                                 const std::string &name,
                                 unsigned seed)
    {
        std::mt19937 rng(seed);
        return make(nb_columns, grid_type, weights_minmax, rng, name);
    }

    /**
     * @brief Construct a triangular-grid DAG with given weights.
     *
     * Lays out vertices in @p nb_columns columns forming a triangular lattice
     * and connects each vertex to the appropriate successors in the next layer.
     *
     * @param nb_columns Number of columns in the triangle.
     * @param weights    Per-vertex weights; length must equal ((nb_columns + 1) * nb_columns) / 2.
     * @param name       Graph name.
     * @return Generated DAG.
     */
    Graph GridDAGGenerator::makeTriangleGridDAG(size_t nb_columns,
                                                const std::vector<weight_t> &weights,
                                                const std::string &name)
    {
        size_t nb_vertex = ((nb_columns + 1) * nb_columns) / 2;
        Graph graph(nb_vertex, name);
        size_t fully_connected_node(0);
        while (nb_columns > 1)
        {
            size_t next_layer_s(fully_connected_node + nb_columns);
            size_t next_layer_f(fully_connected_node + nb_columns * 2 - 1);
            for (auto next_layer_node(next_layer_s); next_layer_node < next_layer_f; next_layer_node++)
            {
                boost::add_edge(fully_connected_node, next_layer_node, graph);
                boost::add_edge(next_layer_node - nb_columns + 1, next_layer_node, graph);
            }
            nb_columns--;
            fully_connected_node = next_layer_s;
        }

        auto weight_map = boost::get(scheduling_problem::vertex_weight_t(), graph);
        for (size_t vertex_id(0); vertex_id < nb_vertex; vertex_id++)
            weight_map[vertex_id] = weights[vertex_id];

        return graph;
    }

    /**
     * @brief Construct a rectangular-grid DAG with given weights.
     *
     * Builds a square grid of size nb_columns × nb_columns using forward edges
     * between adjacent layers in both upper and lower triangular sweeps.
     *
     * @param nb_columns Number of columns and rows.
     * @param weights    Per-vertex weights; length must equal nb_columns*nb_columns.
     * @param name       Graph name.
     * @return Generated DAG.
     */
    Graph GridDAGGenerator::makeRectangleGridDAG(size_t nb_columns,
                                                 const std::vector<weight_t> &weights,
                                                 const std::string &name)
    {
        size_t nb_vertex = nb_columns * nb_columns;
        Graph graph(nb_vertex, name);
        size_t row_size(1);
        while (row_size < nb_columns)
        {
            size_t layer_node_s(((row_size - 1) * row_size) / 2);
            size_t layer_node_f(((row_size + 1) * row_size) / 2);
            for (auto node(layer_node_s); node < layer_node_f; node++)
            {
                boost::add_edge(node, node + row_size, graph);
                boost::add_edge(node, node + row_size + 1, graph);
            }
            row_size++;
        }

        row_size = 1;
        while (row_size < nb_columns)
        {
            size_t layer_node_s(nb_vertex - ((row_size + 1) * row_size) / 2);
            size_t layer_node_f(nb_vertex - ((row_size - 1) * row_size) / 2);
            for (auto node(layer_node_s); node < layer_node_f; node++)
            {
                boost::add_edge(node - row_size, node, graph);
                boost::add_edge(node - row_size - 1, node, graph);
            }
            row_size++;
        }

        auto weight_map = boost::get(scheduling_problem::vertex_weight_t(), graph);
        for (size_t vertex_id(0); vertex_id < nb_vertex; vertex_id++)
            weight_map[vertex_id] = weights[vertex_id];

        return graph;
    }
}
