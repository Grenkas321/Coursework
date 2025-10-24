#include <vector>
#include <future>
#include <thread>
#include "SimulatedAnnealing.h"

namespace scheduling_problem::algorithms
{
    /**
     * Implements simulated anealing in concurrent manner
     */
    class ConcurrentSAO : public BaseOptimization
    {
    public:
        /**
         * Cooling laws (temperature reduction rules).
         */
        typedef SimulatedAnnealing::ReductionRules ReductionRules;
        /**
         * Adjacency matrix for the task graph
         */
        typedef std::vector<std::vector<bool>> AdjacencyMatrix;

    private:
        unsigned partitions_count_, rsearch_iters_;
        bool warm_start_, subareas_;
        randgen rng_;
        ParamSet algo_params_;

    public:
        std::vector<std::vector<weight_t>> conveyor;

    public:
        /**
         * Class constructor
         */
        ConcurrentSAO(unsigned partitions_count = std::thread::hardware_concurrency(),
                      unsigned rsearch_iters = 1000,
                      bool warm_start = true,
                      bool subareas = true,
                      double min_temp = 1,
                      double max_temp = 13,
                      ReductionRules reduction_rule = ReductionRules::boltzmann,
                      unsigned saturation = 0,
                      double improvement = 0,
                      unsigned seed = 42,
                      const std::string &label = "csao");
        /**
         * Copy function
         */
        virtual std::unique_ptr<BaseOptimization> copy() const override;
        /**
         * Get the internal algorithm parameters
         */
        virtual ParamSet getParams() const;
        /**
         * Sets internal parameters
         */
        virtual void setParams(const ParamSet &params);
        /**
         * Create partitions on a given graph and return vector of subgraphs corresponding to partitions
         */
        std::vector<Graph> makePartitions(const Graph &graph);

    protected:
        /**
         * Constructs a schedule for a given graph
         */
        virtual Schedule schedule_(const Graph &graph);
        /**
         * Create adjacency matrix from the given graph
         */
        AdjacencyMatrix makeAdjacencyMatrix(const Graph &graph);
        /**
         * Find the subarea according to adjacency matrix
         */
        std::pair<size_t, size_t> subarea(const AdjacencyMatrix &adjacency_matrix);
    };
}