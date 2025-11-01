#pragma once

#include <set>
#include <string>
#include <filesystem>
#include <unordered_map>
#include "DAGPool.h"

namespace scheduling_problem::additionals
{
    /**
     * Reads given number of graphs from a specified directory
     */
    class DAGReader : public DAGPool
    {
    private:
        std::string directory_;
        std::filesystem::recursive_directory_iterator current_, end_;
        std::vector<std::filesystem::path> paths_;

    public:
        /**
         * Constructor
         */
        DAGReader(std::string directory,
                  unsigned n_samples,
                  unsigned batch_size);

        /**
         * Get next batch of graphs
         */
        virtual Batch nextBatch();

        /**
         * Read next graph from file
         */
        static Graph read(const std::string &filename);

    private:
        static bool isDAG(const std::string &filename);

        static Graph make(const std::string &filename);

        static std::unordered_map<size_t, size_t> cleanData(std::set<size_t> &vertices);
    };
}
