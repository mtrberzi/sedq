#include "trace.h"

#ifdef _TRACE
std::ofstream tout(".sedq-trace");
#endif

void close_trace() {
#ifdef _TRACE
    tout.close();
#endif
}

void open_trace() {
#ifdef _TRACE
    tout.open(".z3-trace");
#endif
}

bool is_trace_enabled(const char * tag) {
    // TODO finer-grained tracing arguments
    return true;
}
