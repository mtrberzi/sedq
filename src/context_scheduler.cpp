#include <cstdlib>
#include <cstdint>
#include "context_scheduler.h"
#include "context.h"
#include "trace.h"

context_priority_cmp::context_priority_cmp(){}

bool context_priority_cmp::operator ()(const Context* lhs, const Context* rhs) {
    int lhs_priority = lhs->get_priority();
    int rhs_priority = rhs->get_priority();
    return lhs_priority < rhs_priority;
}

ContextScheduler::ContextScheduler() : m_maximum_cpu_cycles(0) {}

ContextScheduler::~ContextScheduler() {
    // TODO delete contexts in the run queue and in the list of completed contexts
}

void ContextScheduler::set_maximum_cpu_cycles(uint64_t max_cycles) {
    m_maximum_cpu_cycles = max_cycles;
}

void ContextScheduler::add_context(Context * ctx) {
    m_run_queue.push(ctx);
}

bool ContextScheduler::have_contexts() {
    return !m_run_queue.empty();
}

void ContextScheduler::run_next_context() {
    Context * ctx = m_run_queue.top();
    m_run_queue.pop();

    while (true) {
        ctx->step();
        // check for context forks
        if (ctx->has_forked()) {
            TRACE("scheduler", tout << "Context has forked" << std::endl;);
            m_completed_contexts.push_back(ctx);
            break;
        }
        // check for per-cycle stopping conditions
        if (m_maximum_cpu_cycles != 0 && ctx->get_cpu_cycle_count() >= m_maximum_cpu_cycles) {
            TRACE("scheduler", tout << "Stopping because maximum CPU cycle count was exceeded" << std::endl;);
            m_completed_contexts.push_back(ctx);
            break;
        }
        // TODO check for other per-cycle stopping conditions
        // TODO check for per-frame stopping conditions, once per vblank
    }
}
