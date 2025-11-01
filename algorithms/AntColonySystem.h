#pragma once

#include "IterativeOptimization.h"
#include "ScheduleCorrector.h"
#include "ScheduleStatus.h"
#include "Greedy.h"

namespace scheduling_problem::algorithms
{
    /**
     * Stores information about ant colony algorithm
     */
    class AntColonySystem : public IterativeOptimization
    {

    protected:
        /**
         * Store pheromone matrix
         */
        typedef std::vector<std::vector<double>> Matrix;

        /**
         * Particular version of ant colony algorithm based on greedy approach
         */
        class ArtificialAnt : public Greedy
        {

        public:
            /**
             * Constructor
             */
            ArtificialAnt() = default;

            /**
             * Create route in a graph
             */
            ScheduleStatus makeRoute(const Graph &graph,
                                     Matrix &matrix,
                                     double phe_influence,
                                     double heu_influence,
                                     double threshold,
                                     randgen &rng);

        protected:
            /**
             * Choose next step in route
             */
            size_t choice(const Graph &graph,
                          const Matrix &matrix,
                          const ScheduleStatus &status,
                          size_t vertex_id,
                          double phe_influence,
                          double heu_influence,
                          double threshold,
                          randgen &rng);
        };

        double evaporation_, phe_decay_, phe_influence_, heu_influence_, threshold_;
        unsigned epochs_count_, ants_count_, best_count_;

        /**
         * Store existing ants
         */
        std::vector<ArtificialAnt> ants_;

        /**
         * Regenerate the schedule
         */
        ScheduleCorrector rebuilder_;

    public:
        /**
         * Algorithm constructor
         */
        AntColonySystem(BaseOptimization &baseline = BASELINE,
                        double evaporation = 0.1,
                        double phe_decay = 0.4,
                        double phe_influence = 0.7,
                        double heu_influence = 0.3,
                        double threshold = 0.9,
                        unsigned epochs_count = 200,
                        unsigned ants_count = 5,
                        unsigned best_count = 3,
                        unsigned saturation = 70000,
                        double improvement = 0.0,
                        unsigned seed = 42,
                        const std::string &label = "aco");

        /**
         * Copy constructor
         */
        AntColonySystem(const AntColonySystem &other) = default;

        /**
         * Main method for schedule construction
         */
        Schedule schedule_(const Graph &graph, const Schedule &base_schedule);

        /**
         * Set algorithm parameters
         */
        virtual void setParams(const ParamSet &params);
        /**
         * Get parameters of the algorithm
         */
        virtual ParamSet getParams() const;
        /**
         * Move semantics copy function
         */
        virtual std::unique_ptr<BaseOptimization> copy() const;

    protected:
        /**
         * Create pheromone matrix for the algorithm
         */
        std::vector<std::vector<double>> makeMatrix(size_t n_vertices, double pad) const;

        /**
         * Run algorithm epoch on the given graph with the given matrix
         */
        std::vector<ScheduleStatus> completeEpoch(const Graph &graph,
                                                  Matrix &matrix,
                                                  double init_trail);

        /**
         * Local pheromone updating algorithm
         */
        void localPheUpdate(const ScheduleStatus &solution,
                            Matrix &matrix,
                            double delta = 0.0) const;

        /**
         * Global pheromone updating algorithm
         */
        void globalPheUpdate(const std::vector<ScheduleStatus> &epoch_routes,
                             Matrix &matrix,
                             double weight_max) const;
    };
}
