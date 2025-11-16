#include "GridDAGGenerator.h"

namespace scheduling_problem::additionals
{
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

    Graph GridDAGGenerator::make(size_t nb_columns,
                                 const GridType &grid_type,
                                 const std::pair<size_t, size_t> &weights_minmax,
                                 const std::string &name,
                                 unsigned seed)
    {
        std::mt19937 rng(seed);
        return make(nb_columns, grid_type, weights_minmax, rng, name);
    }

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
