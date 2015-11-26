#include <iostream>
#include <cstdlib>
#include "ast_manager.h"
#include "context.h"
#include "trace.h"

int main(int argc, char *argv[]) {
    open_trace();

    // TODO read arguments

    ASTManager * mgr = new ASTManager_SMT2();
    Context * initial_context = new Context(*mgr);

    // TODO figure out how to fork contexts

    try {
        // this is just a test harness for now
        unsigned int nSteps = 10;
        for (unsigned int i = 0; i < nSteps; ++i) {
            initial_context->step();
        }
    } catch (const char * msg) {
        std::cerr << "exception: " << msg << std::endl;
    }

    delete initial_context;
    delete mgr;

    close_trace();
    return EXIT_SUCCESS;
}
