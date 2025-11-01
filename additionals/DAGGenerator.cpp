#include "DAGGenerator.h"

namespace scheduling_problem::additionals
{
    DAGGenerator::DAGGenerator(const std::vector<std::pair<weight_t, weight_t>> &vert_edge_map,
                               const std::pair<weight_t, weight_t> &weights,
                               unsigned n_samples,
                               unsigned batch_size,
                               unsigned seed,
                               std::string prefix)
        : DAGPool(n_samples, batch_size), vertedge_map_(vert_edge_map), weights_(weights.first, weights.second), rng_(seed), prefix_(prefix)
    {
    }

    DAGPool::Batch DAGGenerator::nextBatch()
    {
        unsigned sample(0);
        Batch graphs(batch_id_++);
        while (current_sample_ < n_samples_ && sample < batch_size_)
        {
            auto graph_name = prefix_ + std::to_string(current_sample_);
            auto graph_params = generateGraphParams();
            graphs.push_back(make(std::get<0>(graph_params),
                                  std::get<1>(graph_params),
                                  std::get<2>(graph_params),
                                  graph_name,
                                  rng_));
            current_sample_++;
            sample++;
        }
        return graphs;
    }

    DAGGenerator::GraphParams DAGGenerator::generateGraphParams()
    {
        auto size = vertedge_map_.size();
        auto pair_num = uniform_weight_t(0, size - 1)(rng_);
        weight_t n_vertex, n_edges;
        std::tie(n_vertex, n_edges) = vertedge_map_[pair_num];
        std::vector<weight_t> weights(n_vertex);
        for (auto &weight : weights)
            weight = weights_(rng_);

        return {n_vertex, n_edges, weights};
    }

    Graph DAGGenerator::make(weight_t n_vertex,
                             weight_t n_edges,
                             const std::vector<weight_t> &weights,
                             const std::string &name,
                             std::mt19937 &rng)
    {
        if (n_vertex <= n_edges - 1)
        {
            Adjacencies adjacencies(n_edges);
            std::unordered_set<weight_t> connections;

            makeConnectedComponent(n_vertex, adjacencies, connections, rng);

            // make remained connections
            weight_t current_edges_count = n_vertex - 1;
            while (current_edges_count < n_edges)
            {
                auto source = uniform_weight_t(0, n_vertex - 2)(rng);
                auto target = uniform_weight_t(source + 1, n_vertex - 1)(rng);
                if (!connections.count(source * n_vertex + target))
                {
                    adjacencies[current_edges_count++] = {source, target};
                    connections.insert(source * n_vertex + target);
                }
            }

            // make graph from adjacency list
            Graph graph(n_vertex, name);

            std::sort(adjacencies.begin(), adjacencies.end());
            for (auto &edge : adjacencies)
                boost::add_edge(edge.first, edge.second, graph);

            auto weight_map = boost::get(vertex_weight_t(), graph);
            for (weight_t vertex_id(0); vertex_id < n_vertex; vertex_id++)
                weight_map[vertex_id] = weights[vertex_id];

            return graph;
        }
        return Graph();
    }

    Graph DAGGenerator::make(weight_t n_vertex,
                             weight_t n_edges,
                             const std::pair<weight_t, weight_t> &weights,
                             const std::string &name,
                             unsigned seed)
    {
        std::mt19937 rng(seed);
        std::uniform_int_distribution<weight_t> weights_gen(weights.first, weights.second);
        std::vector<weight_t> vert_weights(n_vertex);
        for (auto &weight : vert_weights)
            weight = weights_gen(rng);

        return make(n_vertex, n_edges, vert_weights, name, rng);
    }

    void DAGGenerator::makeConnectedComponent(weight_t n_vertex,
                                              Adjacencies &adjacencies,
                                              std::unordered_set<weight_t> &connections,
                                              std::mt19937 &rng)
    {
        std::vector<weight_t> connected, unconnected(n_vertex);
        std::iota(unconnected.begin(), unconnected.end(), 0);

        weight_t vertex = uniform_weight_t(0, n_vertex - 1)(rng);
        connected.push_back(vertex);
        unconnected.erase(unconnected.begin() + vertex);

        while (unconnected.size())
        {
            auto conn_vert = connected[uniform_weight_t(0, connected.size() - 1)(rng)];
            auto unconn_vert_ind = uniform_weight_t(0, unconnected.size() - 1)(rng);
            weight_t source, target;
            std::tie(source, target) = std::minmax({conn_vert, unconnected[unconn_vert_ind]});

            adjacencies[connected.size() - 1] = {source, target};
            connections.insert(source * n_vertex + target);
            connected.push_back(unconnected[unconn_vert_ind]);
            unconnected.erase(unconnected.begin() + unconn_vert_ind);
        }
    }
}
