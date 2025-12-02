#include "DAGGenerator.h"

namespace scheduling_problem::additionals
{
    /**
     * @brief Construct a random DAG generator with predefined size/edge ranges and weights.
     *
     * @param vert_edge_map  Allowed (n_vertices, n_edges) pairs to sample from.
     * @param weights        Inclusive range for vertex weights (min,max).
     * @param n_samples      Total number of graphs to generate.
     * @param batch_size     Number of graphs returned per nextBatch() call.
     * @param seed           RNG seed.
     * @param prefix         Name prefix for generated graphs.
     */
    DAGGenerator::DAGGenerator(const std::vector<std::pair<weight_t, weight_t>> &vert_edge_map,
                               const std::pair<weight_t, weight_t> &weights,
                               unsigned n_samples,
                               unsigned batch_size,
                               unsigned seed,
                               std::string prefix)
        : DAGPool(n_samples, batch_size), vertedge_map_(vert_edge_map), weights_(weights.first, weights.second), rng_(seed), prefix_(prefix)
    {
    }

    /**
     * @brief Generate the next batch of random DAGs.
     *
     * Continues until either @p batch_size_ graphs are created or @p n_samples_
     * is reached. Each graph is named as prefix_ + current_sample_.
     *
     * @return A batch container with generated graphs and a batch id.
     */
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

    /**
     * @brief Sample graph parameters (n_vertices, n_edges, weights) from configured ranges.
     *
     * Picks a (V,E) pair uniformly from @p vertedge_map_ and draws @p V vertex weights
     * from the uniform integer distribution @p weights_.
     *
     * @return Tuple {n_vertex, n_edges, weights_vector}.
     */
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

    /**
     * @brief Build a random connected acyclic graph with given vertices, edges and fixed weights.
     *
     * Produces a DAG by first creating a connected backbone of V-1 edges, then adding
     * extra forward edges (source < target) uniformly without duplicates until E edges exist.
     * Vertex weights are assigned from @p weights. Returns an empty graph if the
     * (V,E) constraint is invalid (requires V > E - 1).
     *
     * @param n_vertex  Number of vertices V.
     * @param n_edges   Number of edges E.
     * @param weights   Per-vertex weights (size V).
     * @param name      Graph name.
     * @param rng       RNG to use.
     * @return Generated DAG or an empty graph on invalid parameters.
     */
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

    /**
     * @brief Build a random DAG with vertex weights sampled from a range.
     *
     * Convenience overload that samples integer weights uniformly in [weights.first, weights.second],
     * then forwards to the main @p make overload.
     *
     * @param n_vertex  Number of vertices V.
     * @param n_edges   Number of edges E.
     * @param weights   Inclusive integer range for vertex weights.
     * @param name      Graph name.
     * @param seed      RNG seed (local to this call).
     * @return Generated DAG or an empty graph on invalid parameters.
     */
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

    /**
     * @brief Create a connected backbone (tree) of V-1 forward edges.
     *
     * Randomly grows a connected component by linking a random already-connected vertex
     * to a random unconnected vertex with a forward edge (source < target). Records
     * edges into @p adjacencies[0..V-2] and memoizes the pair identifiers into @p connections.
     *
     * @param n_vertex     Number of vertices V.
     * @param adjacencies  Output adjacency list (size >= V-1) to fill with tree edges.
     * @param connections  Set of encoded pairs source*V + target to prevent duplicates.
     * @param rng          RNG to use.
     */
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
