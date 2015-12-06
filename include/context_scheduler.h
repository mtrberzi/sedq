#ifndef _CONTEXT_SCHEDULER_H_
#define _CONTEXT_SCHEDULER_H_

#include "context.h"
#include <queue>
#include <vector>
#include <cstdint>

class Context;

class context_priority_cmp {
public:
    context_priority_cmp();
    bool operator() (const Context* lhs, const Context* rhs);
};

class ContextScheduler {
public:
    ContextScheduler();
    virtual ~ContextScheduler();

    void set_maximum_cpu_cycles(uint64_t max_cycles);

    void add_context(Context * ctx);
    void run_next_context();
    bool have_contexts();
protected:
    std::priority_queue<Context*, std::vector<Context*>, context_priority_cmp> m_run_queue;
    std::vector<Context*> m_completed_contexts;

    uint64_t m_maximum_cpu_cycles;
};

#endif // _CONTEXT_SCHEDULER_H_
