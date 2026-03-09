#pragma once

#include <set>
#include <string>
#include <filesystem>
#include <unordered_map>
#include <vector>
#include "DAGPool.h"
#include "general_types.h"

namespace scheduling_problem::additionals
{
    /**
     * @brief Reader of DAG files from a directory (batch-capable).
     *
     * Supports two text formats:
     *  1) Legacy: header line "node size children" followed by lines
     *     "id size child1 child2 ...".
     *  2) New: lines "id  w1: c1 c2,  w2: c3 ..." where each comma-separated
     *     group defines a buffer with weight w_k and consumers c_i.
     */
    class DAGReader : public DAGPool
    {
    private:
        /** @brief Root directory to scan recursively. */
        std::string directory_;
        /** @brief Collected file paths (sorted). */
        std::vector<std::filesystem::path> paths_;
        /** @brief Number of successfully emitted DAG samples. */
        unsigned emitted_samples_ = 0;

    public:
        /**
         * @brief Construct a reader and pre-scan the directory tree.
         *
         * The constructor recursively enumerates files under @p directory and
         * caps the total sample count to the number of discovered files.
         *
         * @param directory  Root directory to scan.
         * @param n_samples  Maximum number of graphs to read.
         * @param batch_size Number of graphs returned per nextBatch() call.
         */
        DAGReader(std::string directory,
                  unsigned n_samples,
                  unsigned batch_size);

        /**
         * @brief Parse the next batch of graphs from disk.
         *
         * Walks @p paths_ in order, parses files recognized as a supported DAG
         * format, and returns up to @p batch_size graphs (or fewer if exhausted).
         *
         * @return Batch containing parsed graphs and a batch id.
         */
        virtual Batch nextBatch();

        /**
         * @brief Read and parse a single graph from @p filename.
         *
         * @param filename Path to file.
         * @return Parsed graph, or an empty graph if the format is not supported.
         */
        static Graph read(const std::string &filename);

    private:
        /**
         * @brief Heuristic detection of a supported DAG format.
         *
         * Recognizes:
         *  - Legacy header "node size children".
         *  - New format with optional header; first meaningful line either
         *    starts with "prog_id" (case-insensitive) or with an integer id.
         *
         * @param filename Path to file.
         * @return True if the file appears to contain a supported DAG format.
         */
        static bool isDAG(const std::string &filename);

        /**
         * @brief Format-dispatching graph constructor.
         *
         * Inspects the first non-empty line and dispatches to the appropriate
         * parser (legacy or new).
         *
         * @param filename Path to input file.
         * @return Parsed graph, or an empty graph on failure.
         */
        static Graph make(const std::string &filename);

        /**
         * @brief Parse legacy format: header "node size children" then "id weight children...".
         *
         * Builds a normalized graph where vertex ids are compacted to [0..N),
         * vertex weights are set from the input, and edges are created with
         * buffer_id = 0 and edge_weight equal to the parent vertex weight.
         *
         * @param file      Open stream positioned at the beginning.
         * @param filename  Full path used to derive the graph name.
         * @return Parsed graph.
         */
        static Graph makeOld(std::ifstream &file, const std::string &filename);

        /**
         * @brief Parse new format: "id  w1: c1 c2,  w2: c3 ...".
         *
         * For each vertex, sums buffer weights to obtain its vertex weight and
         * creates edges for each buffer group with a shared buffer_id and
         * edge_weight equal to that group's weight.
         *
         * @param file      Open stream positioned at the beginning.
         * @param filename  Full path used to derive the graph name.
         * @return Parsed graph.
         */
        static Graph makeNew(std::ifstream &file, const std::string &filename);

        /**
         * @brief Compact arbitrary vertex ids (possibly negative) to [0..N).
         *
         * Assigns consecutive normalized ids in ascending order of raw ids.
         *
         * @param vertices Set of raw vertex ids encountered in the file.
         * @return Mapping {raw_id -> normalized_id}.
         */
        static std::unordered_map<long long, size_t> cleanData(std::set<long long> &vertices);
    };
}
