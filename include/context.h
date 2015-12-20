#ifndef _CONTEXT_H_
#define _CONTEXT_H_

#include <fstream>
#include <cstdint>
#include <vector>
#include <map>
#include "expression.h"
#include "ast_manager.h"
#include "context_scheduler.h"

class Mapper;
class ContextScheduler;

#define MAX_PRG_ROM_SIZE (0x800)
#define MAX_CHR_ROM_SIZE (0x1000)

class Context;

typedef void (*FCPUWrite) (Context & ctx, uint8_t bank, uint16_t addr, Expression * val);
typedef Expression* (*FCPURead) (Context & ctx, uint8_t bank, uint16_t addr);

// enum to track which device we're going to step next
enum EDevice {
    Device_CPU,
};

// TODO enum-to-string

// enum to track the CPU's execution state
enum ECPUState {
    CPU_Reset1, CPU_Reset2, CPU_Reset3, CPU_Reset4, CPU_Reset5, CPU_Reset6, CPU_Reset7, CPU_Reset8,
    CPU_Decode, CPU_AddressingMode, CPU_Execute
};

// enum to track which addressing mode we are executing

enum ECPUAddressingMode {
	CPU_AM_IMP, CPU_AM_IMM, CPU_AM_ABS, CPU_AM_REL,
	CPU_AM_ABX, CPU_AM_ABXW, CPU_AM_ABY, CPU_AM_ABYW,
	CPU_AM_ZPG, CPU_AM_ZPX, CPU_AM_ZPY, CPU_AM_INX,
	CPU_AM_INY, CPU_AM_INYW, CPU_AM_NON
};

class Context {
public:
    // create "reset" context
    Context(ASTManager & m, ContextScheduler & sch);
    // create inherited context
    Context(ASTManager & m, Context * parent);
    virtual ~Context();

    void load_iNES(std::istream & in);

    ASTManager & get_manager();
    ContextScheduler & get_scheduler();

    int get_priority() const;
    bool has_forked() const;

    void step();

    // CPU
    uint64_t get_cpu_cycle_count();
    void step_cpu();
    void cpu_reset();
    void cpu_read(Expression * address);
    void cpu_write(Expression * address, Expression * data);

    Expression * get_cpu_A();
    Expression * get_cpu_X();
    Expression * get_cpu_Y();
    Expression * get_cpu_SP();
    Expression * get_cpu_PC();

    Expression * get_cpu_FN();
    Expression * get_cpu_FV();
    Expression * get_cpu_FD();
    Expression * get_cpu_FI();
    Expression * get_cpu_FZ();
    Expression * get_cpu_FC();

    Expression ** get_cpu_RAM();
    Expression * cpu_read_ram(uint16_t addr);
    void cpu_write_ram(uint16_t addr, Expression * value);
    Expression *** get_cpu_PRG_pointer();
    bool * get_cpu_readable();
    bool * get_cpu_writable();
    Expression * get_cpu_address();
    Expression * get_cpu_last_read();

    Expression *** get_cpu_PRG_ROM();
    uint32_t get_prg_mask_rom();

    // Controller
    void controller_write(Expression * val);
    Expression * controller_read1();
    Expression * controller_read2();
    std::vector<Expression*> & get_controller1_inputs();

protected:
    ASTManager & m;
    ContextScheduler & sch;
    Context * m_parent_context;
    bool m_has_forked;

    uint64_t m_step_count;

    EDevice m_next_device;

    uint32_t m_frame_number;

    /* *
     * ******
     * Mapper
     * ******
     * */

    Mapper * m_mapper;

    uint32_t m_mapper_prg_size_rom;
    uint32_t m_mapper_prg_size_ram;
    uint32_t m_mapper_chr_size_rom;
    uint32_t m_mapper_chr_size_ram;

    Expression *** m_PRG_ROM;
    Expression *** m_CHR_ROM;

    /* *
     * ***
     * CPU
     * ***
     * */
    uint64_t m_cpu_cycle_count;
    ECPUState m_cpu_state;
    ECPUAddressingMode m_cpu_addressing_mode_state;
    uint8_t m_cpu_addressing_mode_cycle;
    bool m_cpu_memory_phase;
    uint8_t m_cpu_current_opcode;
    uint8_t m_cpu_execute_cycle;
    void instruction_fetch();
    bool decode_addressing_mode();
    Expression * m_cpu_calc_addr;
    Expression * m_cpu_branch_offset;
    void cpu_addressing_mode_cycle();
    void cpu_execute();
    void cpu_branch(Expression * condition);

    void increment_PC();

    // macros for flags in P
    void cpu_set_FC(Expression * test);
    void cpu_set_FZ(Expression * test);
    void cpu_set_FN(Expression * test);

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

    //Expression * m_cpu_ram[0x800];
    Expression ** m_cpu_ram;
    std::map<uint16_t, Expression*> m_cpu_ram_copyonwrite;

    /* *
     * ***********
     * Controllers
     * ***********
     * */

    // for now we assume that the only thing we'll ever hook up is a standard controller.
    // I don't want to think about the consequences of symbolically simulating the zapper, for instance...

    // standard controller reads buttons in the order A B Select Start Up Down Left Right

    Expression * m_controller1_bits;
    uint8_t m_controller1_bit_ptr;
    bool m_controller1_strobe;
    uint32_t m_controller1_seqno;
    std::vector<Expression*> m_controller1_inputs;

    Expression * controller_mk_var(int controller_number);

};

#endif // _CONTEXT_H_
