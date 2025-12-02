#include "Greedy.h"

namespace scheduling_problem::algorithms {

/**
 * Global baseline optimizer instance used by multiple algorithms
 * (e.g., RandomSearch, ConcurrentSAO, AntColonySystem) for seeding
 * and fallbacks.
 *
 * Label: "BASELINE".
 */
Greedy BASELINE("BASELINE");

}
