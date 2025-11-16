#include "BaseOptimization.h"
#include "ScheduleStatus.h"
#include "ToOneSourceTarget.h"
#include "ToEdgeMemory.h"
#include "FindSeriesParallel.h"
#include "CumWeight.h"
#include "ScheduleSPGraph.h"
#include "GreedyEdgeVertexInsertion.h"

#include <boost/graph/graphviz.hpp>
#include <boost/graph/graph_utility.hpp>

namespace scheduling_problem::algorithms
{
    /**
     * Implements the algorithm based on series-parallel graphs
     */
    class SeriesParallel : public virtual BaseOptimization
    {

    public:
        /**
         * Constructor to create empty instance of algorithm
         * \param label Name of the algorithm
         */
        SeriesParallel(const std::string &label = "sp");

        /**
         * Copy constructor
         */
        SeriesParallel(const SeriesParallel &other) = default;

        /**
         *
         * Copy constructor
         */
        virtual std::unique_ptr<BaseOptimization> copy() const override;

    protected:
        /**
         * Invoke schedule computation using series-parallel method
         * \param graph Graph for which the schedule is being constructed
         * \return Finally computed %schedule for the graph
         */
        virtual Schedule schedule_(const Graph &graph);
    };
}
