#include "ScheduleStatus.h"
#include <algorithm>

namespace scheduling_problem
{

// =========================================================
//  CONSTRUCTOR 1: Empty schedule for graph
// =========================================================
ScheduleStatus::ScheduleStatus(const Graph &graph, bool high_density)
    : Schedule(),                                      // ← ПУСТОЙ Schedule
      positions_(boost::num_vertices(graph), NOT_PRESENT),
      use_external_mem_(high_density)
{
    if (use_external_mem_)
    {
        auto n = boost::num_vertices(graph);

        child_remain_.assign(n, 0);
        release_on_.assign(n, nullptr);

        for (auto v : boost::make_iterator_range(boost::vertices(graph)))
        {
            unsigned cnt = 0;
            for (auto e : boost::make_iterator_range(boost::out_edges(v, graph)))
                if (boost::get(edge_kind_t(), graph, e) == EdgeKind::Real)
                    cnt++;

            child_remain_[v] = cnt;
        }
    }
}

// =========================================================
//  CONSTRUCTOR 2: Build schedule from existing schedule
// =========================================================
ScheduleStatus::ScheduleStatus(const Graph &graph,
                               const Schedule &schedule,
                               bool high_density)
    : ScheduleStatus(graph, high_density)     // ← вызывает пустой конструктор
{
    weight_t target = 0;

    for (size_t pos = 0; pos < schedule.size(); ++pos)
    {
        auto v = schedule[pos].id;
        auto w = boost::get(vertex_weight_t(), graph, v);

        weight_t rel = children(v, graph).empty() ? w : 0;

        // --- ADD job ---
        push_back(Job(v, w, rel));

        // === ВАЖНО ===
        // Корректно прописываем позицию добавленной вершины
        positions_[v] = pos;

        // cost_ update
        target += w;
        if (cost_ < target) cost_ = target;

        // release accounting
        for (auto p : parents(v, graph))
            if (releasePosition(p, graph).first)
                rel += boost::get(vertex_weight_t(), graph, p);

        target -= rel;

        if (use_external_mem_)
        {
            for (auto p : parents(v, graph))
            {
                child_remain_[p]--;
                release_on_[p] = &positions_[v];
            }
        }
    }
}

// =========================================================
//  COPY CONSTRUCTOR
// =========================================================
ScheduleStatus::ScheduleStatus(const ScheduleStatus &other)
    : Schedule(other),
      positions_(other.positions_),
      child_remain_(other.child_remain_),
      use_external_mem_(other.use_external_mem_)
{
    if (use_external_mem_)
    {
        release_on_.assign(positions_.size(), nullptr);

        for (const auto &job : other)
        {
            auto rp = other.release_on_[job.id];
            if (rp)
            {
                size_t release_vid = *rp;
                release_on_[job.id] = &positions_[release_vid];
            }
        }
    }
}

// =========================================================
// ACCESSORS
// =========================================================

Job &ScheduleStatus::operator[](size_t pos) { return Schedule::operator[](pos); }
const Job &ScheduleStatus::operator[](size_t pos) const { return Schedule::operator[](pos); }

size_t ScheduleStatus::size() const { return Schedule::size(); }

size_t ScheduleStatus::loc(size_t vid) const
{
    return positions_[vid];
}

// =========================================================
//  lower / upper bounds
// =========================================================

size_t ScheduleStatus::upper(size_t curr_vid, const Graph &graph) const
{
    if (size() == 0) return 1;

    size_t ub = size();

    for (auto c : children(curr_vid, graph))
        if (contains(c) && positions_[c] < ub)
            ub = positions_[c];

    return ub;
}

size_t ScheduleStatus::lower(size_t curr_vid, const Graph &graph) const
{
    size_t lb = 0;

    for (auto p : parents(curr_vid, graph))
        if (contains(p) && positions_[p] >= lb)
            lb = positions_[p] + 1;

    return lb;
}

// =========================================================
//  release helpers
// =========================================================

std::pair<size_t, size_t>
ScheduleStatus::releaseOn(size_t curr_vid, const Graph &graph) const
{
    size_t last_child = 0, child_remain = 0;

    for (auto c : children(curr_vid, graph))
    {
        if (contains(c))
        {
            if (positions_[c] > last_child)
                last_child = positions_[c];
        }
        else child_remain++;
    }
    return {child_remain, last_child};
}

// =========================================================
//  parents / children
// =========================================================

std::vector<size_t> ScheduleStatus::parents(size_t curr_vid, const Graph &graph) const
{
    std::vector<size_t> res;
    for (auto e : boost::make_iterator_range(boost::in_edges(curr_vid, graph)))
        if (boost::get(edge_kind_t(), graph, e) == EdgeKind::Real)
            res.push_back(boost::source(e, graph));
    return res;
}

std::vector<size_t> ScheduleStatus::children(size_t curr_vid, const Graph &graph) const
{
    std::vector<size_t> res;
    for (auto e : boost::make_iterator_range(boost::out_edges(curr_vid, graph)))
        if (boost::get(edge_kind_t(), graph, e) == EdgeKind::Real)
            res.push_back(boost::target(e, graph));
    return res;
}

// =========================================================
// contains()
// =========================================================

bool ScheduleStatus::contains(size_t vid) const
{
    if (vid >= positions_.size()) return false;
    return positions_[vid] != NOT_PRESENT;
}

// =========================================================
// INSERT
// =========================================================

void ScheduleStatus::insert(size_t curr_vid, size_t pos, const Graph &graph)
{
    auto weights = boost::get(vertex_weight_t(), graph);
    weight_t rel = 0;

    // insert new job
    std::vector<Job>::insert(begin() + pos, Job(curr_vid, weights[curr_vid], rel));

    // update ALL positions
    for (size_t p = pos; p < size(); ++p)
        positions_[operator[](p).id] = p;

    cost(graph);
}

// =========================================================
// MOVE
// =========================================================

weight_t ScheduleStatus::move(size_t curr_vid, size_t tpos, const Graph &graph)
{
    // IMPORTANT: keep schedule topologically valid.
    // NOTE: upper(curr_vid) is an *insertion* upper bound (place BEFORE the earliest child).
    // For MOVE to the right, putting a parent at the child's index makes it end up AFTER the child.
    if (!contains(curr_vid))
        return cost(false);

    size_t curr_pos = positions_[curr_vid];
    const size_t lb = lower(curr_vid, graph);

    const size_t ub_raw = upper(curr_vid, graph);
    size_t ub = ub_raw;
    if (ub >= size())
        ub = size() ? size() - 1 : 0;

    // Moving right: must be STRICTLY before earliest child (if any child exists).
    if (curr_pos < tpos && ub_raw < size())
    {
        if (ub_raw == 0)
            tpos = 0;
        else if (tpos >= ub_raw)
            tpos = ub_raw - 1;
    }

    // Clamp into feasible window.
    if (tpos < lb) tpos = lb;
    if (tpos > ub) tpos = ub;

    if (curr_pos == tpos)
        return cost(false);

    if (curr_pos < tpos)
        std::rotate(begin() + curr_pos, begin() + curr_pos + 1, begin() + tpos + 1);
    else
        std::rotate(begin() + tpos, begin() + curr_pos, begin() + curr_pos + 1);

    updatePositions(std::min(curr_pos, tpos), std::max(curr_pos, tpos));
    return cost(graph);
}


// =========================================================
// updatePositions
// =========================================================

inline void ScheduleStatus::updatePositions(size_t spos, size_t fpos)
{
    for (size_t pos = spos; pos <= fpos; ++pos)
        positions_[operator[](pos).id] = pos;
}

// =========================================================
// releasePosition
// =========================================================

inline std::pair<bool, size_t>
ScheduleStatus::releasePosition(size_t curr_vid, const Graph &graph) const
{
    size_t release_pos = 0;

    for (auto c : children(curr_vid, graph))
    {
        if (!contains(c)) return {false, 0};
        if (positions_[c] > release_pos)
            release_pos = positions_[c];
    }
    return {true, release_pos};
}

// =========================================================
// prevReleasePosition
// =========================================================

inline std::pair<bool, size_t>
ScheduleStatus::prevReleasePosition(size_t curr_vid,
                                    const Graph &graph,
                                    size_t target_pos) const
{
    size_t prev = 0;

    for (auto c : children(curr_vid, graph))
    {
        if (!contains(c)) return {false, 0};
        if (positions_[c] != target_pos && prev < positions_[c])
            prev = positions_[c];
    }
    return {true, prev};
}

// =========================================================
// lmove / rmove  (unchanged, but safe now)
// =========================================================

inline void ScheduleStatus::lmove(size_t /*curr_vid*/,
                                  size_t curr_pos,
                                  size_t target_pos,
                                  const Graph & /*graph*/)
{
    size_t rcurr = size() - curr_pos - 1;
    size_t rtarg = size() - target_pos - 1;
    std::rotate(rbegin() + rcurr, rbegin() + rcurr + 1, rbegin() + rtarg + 1);
    updatePositions(target_pos, curr_pos);
}

inline void ScheduleStatus::rmove(size_t /*curr_vid*/,
                                  size_t curr_pos,
                                  size_t target_pos,
                                  const Graph & /*graph*/)
{
    std::rotate(begin() + curr_pos, begin() + curr_pos + 1, begin() + target_pos + 1);
    updatePositions(curr_pos, target_pos);
}

} // namespace scheduling_problem
