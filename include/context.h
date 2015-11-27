#ifndef _CONTEXT_H_
#define _CONTEXT_H_

#include <cstdint>
#include "expression.h"
#include "ast_manager.h"

class Context;

typedef void (*FCPUWrite) (Context & ctx, uint8_t bank, uint16_t addr, Expression * val);
typedef Expression* (*FCPURead) (Context & ctx, uint8_t bank, uint16_t addr);

// enum to track which device we're going to step next
enum EDevice {
    Device_CPU,
};

// enum to track the CPU's execution state
enum ECPUState {
    CPU_Reset1, CPU_Reset2, CPU_Reset3, CPU_Reset4, CPU_Reset5, CPU_Reset6, CPU_Reset7, CPU_Reset8,
    CPU_Decode,
};

class Context {
public:
    // create "reset" context
    Context(ASTManager & m);
    // create inherited context
    Context(ASTManager & m, Context * parent);
    virtual ~Context();

    ASTManager & get_manager();

    void step();

    // CPU
    void step_cpu();
    void cpu_reset();
    void cpu_read(Expression * address);
    void cpu_write(Expression * address, Expression * data);

    Expression * get_cpu_A();
    Expression * get_cpu_X();
    Expression * get_cpu_Y();
    Expression * get_cpu_SP();
    Expression * get_cpu_PC();

    Expression ** get_cpu_RAM();
    Expression *** get_cpu_PRG_pointer();
    bool * get_cpu_readable();
    bool * get_cpu_writable();
    Expression * get_cpu_address();

protected:
    ASTManager & m;
    Context * m_parent_context;

    uint64_t m_step_count;

    EDevice m_next_device;

    /* *
     * ***
     * CPU
     * ***
     * */
    ECPUState m_cpu_state;

    FCPURead m_cpu_read_handler[0x10];
    FCPUWrite m_cpu_write_handler[0x10];
    Expression ** m_cpu_prg_pointer[0x10];
    bool m_cpu_readable[0x10];
    bool m_cpu_writable[0x10];

    bool m_cpu_want_nmi;
    bool m_cpu_want_irq;
    uint8_t m_cpu_pcm_cycles;

    // CPU registers
    Expression * m_cpu_A; // 8 bits
    Expression * m_cpu_X; // 8 bits
    Expression * m_cpu_Y; // 8 bits
    Expression * m_cpu_SP; // 8 bits
    Expression * m_cpu_PC; // 16 bits
    // We don't store the P register per se;
    // instead we track each bit separately
    // P = [7] N V - - D I Z C [0]
    // *** NOTE THAT THESE ARE BOOLEANS ***
    Expression * m_cpu_FC;
    Expression * m_cpu_FZ;
    Expression * m_cpu_FI;
    Expression * m_cpu_FD;
    Expression * m_cpu_FV;
    Expression * m_cpu_FN;

    // CPU address bus
    Expression * m_cpu_last_read;
    Expression * m_cpu_address;
    bool m_cpu_write_enable;
    Expression * m_cpu_data_out;

    Expression * m_cpu_ram[0x800];

};

#endif // _CONTEXT_H_
