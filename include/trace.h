#ifndef _TRACE_H_
#define _TRACE_H_

#include <fstream>
#include <iostream>

#ifdef _TRACE
//extern std::ofstream tout;
extern std::ostream & tout;
#define TRACE_CODE(CODE) { CODE } ((void) 0 )
#else
#define TRACE_CODE(CODE) ((void) 0)
#endif

#define TRACE(TAG, CODE) TRACE_CODE(if (is_trace_enabled(TAG)) { tout << "-------- [" << TAG << "] " << __FUNCTION__ << " " << __FILE__ << ":" << __LINE__ << " ---------\n"; CODE tout << "------------------------------------------------\n"; tout.flush(); })

#define STRACE(TAG, CODE) TRACE_CODE(if (is_trace_enabled(TAG)) { CODE tout.flush(); })

#define CTRACE(TAG, COND, CODE) TRACE_CODE(if (is_trace_enabled(TAG) && (COND)) { tout << "-------- [" << TAG << "] " << __FUNCTION__ << " " << __FILE__ << ":" << __LINE__ << " ---------\n"; CODE tout << "------------------------------------------------\n"; tout.flush(); })

bool is_trace_enabled(const char * tag);
void close_trace();
void open_trace();

#endif // _TRACE_H_
