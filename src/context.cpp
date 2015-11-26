#include <cstdlib>
#include <cstdint>
#include "context.h"
#include "ast_manager.h"
#include "trace.h"

static Expression * CPU_ReadRAM(Context & ctx, uint8_t bank, uint16_t addr) {
    return ctx.get_cpu_RAM()[addr & 0x07FF];
}

static void CPU_WriteRAM(Context & ctx, uint8_t bank, uint16_t addr, Expression * val) {
    ctx.get_cpu_RAM()[addr & 0x07FF] = val;
}

static Expression * PPU_IntRead(Context & ctx, uint8_t bank, uint16_t addr) {
    // TODO PPU_IntRead
    return NULL;
}

static void PPU_IntWrite(Context & ctx, uint8_t bank, uint16_t addr, Expression * val) {
    // TODO PPU_IntWrite
}

static Expression * APU_IntRead(Context & ctx, uint8_t bank, uint16_t addr) {
    // TODO APU_IntRead
    return NULL;
}

static void APU_IntWrite(Context & ctx, uint8_t bank, uint16_t addr, Expression * val) {
    // TODO APU_IntWrite
}

static Expression * CPU_ReadPRG(Context & ctx, uint8_t bank, uint16_t addr) {
    if (ctx.get_cpu_readable()[bank]) {
        return ctx.get_cpu_PRG_pointer()[bank][addr];
    } else {
        return ctx.get_manager().mk_byte(0xFF);
    }
}

static void CPU_WritePRG(Context & ctx, uint8_t bank, uint16_t addr, Expression * val) {
    if (ctx.get_cpu_writable()[bank]) {
        ctx.get_cpu_PRG_pointer()[bank][addr] = val;
    }
}

Context::Context(ASTManager & m)
: m(m), m_parent_context(NULL), m_next_device(EDevice::Device_CPU),
  // CPU
  m_cpu_state(ECPUState::CPU_Reset1),
  m_cpu_A(m.mk_byte(0)), m_cpu_X(m.mk_byte(0)), m_cpu_Y(m.mk_byte(0)), m_cpu_SP(m.mk_byte(0)), m_cpu_PC(m.mk_halfword(0)),
  m_cpu_FC(m.mk_byte(0)), m_cpu_FZ(m.mk_byte(0)), m_cpu_FI(m.mk_byte(0)),
  m_cpu_FD(m.mk_byte(0)), m_cpu_FV(m.mk_byte(0)), m_cpu_FN(m.mk_byte(0)),
  m_cpu_last_read(m.mk_byte(0)),
  // start the read for Reset1
  m_cpu_address(m.mk_halfword(0)), m_cpu_write_enable(false), m_cpu_data_out(m.mk_byte(0))
{
    // *** CPU initialization ***

    // CPU read/write handlers
    for (unsigned int i = 0; i < 0x10; ++i) {
        m_cpu_read_handler[i] = CPU_ReadPRG;
        m_cpu_write_handler[i] = CPU_WritePRG;
        m_cpu_readable[i] = false;
        m_cpu_writable[i] = false;
    }

    m_cpu_read_handler[0] = CPU_ReadRAM; m_cpu_write_handler[0] = CPU_WriteRAM;
    m_cpu_read_handler[1] = CPU_ReadRAM; m_cpu_write_handler[1] = CPU_WriteRAM;
    m_cpu_read_handler[2] = PPU_IntRead; m_cpu_write_handler[2] = PPU_IntWrite;
    m_cpu_read_handler[3] = PPU_IntRead; m_cpu_write_handler[3] = PPU_IntWrite;

    // TODO special check for vs. unisystem roms

    m_cpu_read_handler[4] = APU_IntRead; m_cpu_write_handler[4] = APU_IntWrite;

    // zero RAM
    for (unsigned int i = 0; i < 0x800; ++i) {
        m_cpu_ram[i] = m.mk_byte(0);
    }
}

Context::Context(ASTManager & m, Context * parent)
: m(m), m_parent_context(parent)
{
    // *** CPU initialization ***

    // null out RAM
    for (unsigned int i = 0; i < 0x800; ++i) {
        m_cpu_ram[i] = NULL;
    }
}

Context::~Context() {

}

ASTManager & Context::get_manager() {
    return m;
}

// TODO front-half read() and write() force a switch to the next peripheral
// see MemGet() and MemSet()

void Context::cpu_read(Expression * address) {
    m_cpu_address = address;
    m_cpu_write_enable = false;
}

void Context::cpu_write(Expression * address, Expression * data) {
    m_cpu_address = address;
    m_cpu_write_enable = true;
    m_cpu_data_out = data;
}

void Context::step() {
    switch (m_next_device) {
    case Device_CPU:
    {
        TRACE("step", tout << "stepping CPU" << std::endl;);
        step_cpu();
    } break;
    }

}

Expression * Context::get_cpu_A() {
    if (m_cpu_A == NULL) {
        m_cpu_A = m_parent_context->get_cpu_A();
    }
    return m_cpu_A;
}

Expression * Context::get_cpu_X() {
    if (m_cpu_X == NULL) {
        m_cpu_X = m_parent_context->get_cpu_X();
    }
    return m_cpu_X;
}

Expression * Context::get_cpu_Y() {
    if (m_cpu_Y == NULL) {
        m_cpu_Y = m_parent_context->get_cpu_Y();
    }
    return m_cpu_Y;
}

Expression * Context::get_cpu_SP() {
    if (m_cpu_SP == NULL) {
        m_cpu_SP = m_parent_context->get_cpu_SP();
    }
    return m_cpu_SP;
}

Expression * Context::get_cpu_PC() {
    if (m_cpu_PC == NULL) {
        m_cpu_PC = m_parent_context->get_cpu_PC();
    }
    return m_cpu_PC;
}

Expression ** Context::get_cpu_RAM() {
    return m_cpu_ram;
}

bool * Context::get_cpu_readable() {
    return m_cpu_readable;
}

bool * Context::get_cpu_writable() {
    return m_cpu_writable;
}

Expression *** Context::get_cpu_PRG_pointer() {
    return m_cpu_prg_pointer;
}

Expression * Context::get_cpu_address() {
    if (m_cpu_address == NULL) {
        m_cpu_address = m_parent_context->get_cpu_address();
    }
    return m_cpu_address;
}

void Context::cpu_reset() {
    switch (m_cpu_state) {
    /*
     * The reset sequence is
     * MemGetCode(PC)
     * MemGetCode(PC)
     * MemGet(0x100 | SP--)
     * MemGet(0x100 | SP--)
     * MemGet(0x100 | SP--)
     * FI = 1
     * PC[7:0] = MemGet(0xFFFC)
     * PC[15:8] = MemGet(0xFFFD)
     * Opcode = MemGetCode(OpAddr = PC++)
     * then into instruction decode
     */
        // MemGetCode(PC);
    case CPU_Reset1:
        // MemGetCode(PC);
        cpu_read(get_cpu_PC());
        m_cpu_state = CPU_Reset2;
        break;
    case CPU_Reset2:
        // MemGet(0x100 | SP);
        cpu_read(m.mk_bv_or(
                    m.mk_halfword(0x0100),
                    m.mk_bv_concat(
                            m.mk_byte(0),
                            get_cpu_SP())));
        m_cpu_state = CPU_Reset3;
        break;
    case CPU_Reset3:
        // SP -= 1;
        // MemGet(0x100 | SP);
        m_cpu_SP = m.mk_bv_sub(get_cpu_SP(), m.mk_byte(1));
        cpu_read(m.mk_bv_or(
                            m.mk_halfword(0x0100),
                            m.mk_bv_concat(
                                    m.mk_byte(0),
                                    get_cpu_SP())));
        m_cpu_state = CPU_Reset4;
        break;
    case CPU_Reset4:
        // SP -= 1;
        // MemGet(0x100 | SP);
        m_cpu_SP = m.mk_bv_sub(get_cpu_SP(), m.mk_byte(1));
                cpu_read(m.mk_bv_or(
                                    m.mk_halfword(0x0100),
                                    m.mk_bv_concat(
                                            m.mk_byte(0),
                                            get_cpu_SP())));
        m_cpu_state = CPU_Reset5;
        break;
    case CPU_Reset5:
        // SP -= 1;
        // FI = 1;
        // MemGet(0xFFFC)
        m_cpu_SP = m.mk_bv_sub(get_cpu_SP(), m.mk_byte(1));
        m_cpu_FI = m.mk_bool(true);
        cpu_read(m.mk_halfword(0xFFFC));
        m_cpu_state = CPU_Reset6;
        break;
    case CPU_Reset6:
        // PC[7:0] = data_in
        m_cpu_PC = m.mk_bv_concat(
                m.mk_bv_extract(get_cpu_PC(), m.mk_int(15), m.mk_int(8)),
                m_cpu_last_read);
        // MemGet(0xFFFD)
        cpu_read(m.mk_halfword(0xFFFD));
        m_cpu_state = CPU_Reset7;
        break;
    case CPU_Reset7:
        // PC[15:8] = data_in
        // MemGetCode(PC);
        m_cpu_PC = m.mk_bv_concat(
                        m_cpu_last_read,
                        m.mk_bv_extract(get_cpu_PC(), m.mk_int(7), m.mk_int(0))
                        );
        cpu_read(get_cpu_PC());
        m_cpu_state = CPU_Decode;
        break;
    }
}

void Context::step_cpu() {
    // CPU steps always start by completing the memory access from the previous step
    uint16_t address;
    // deal with the address right away
    if (get_cpu_address()->is_concrete()) {
        address = (uint16_t) (get_cpu_address()->get_value() & 0x0000FFFF);
    } else {
        // oh no. symbolic address.
        // TODO as soon as I figure out how context forks will work
        throw "oops, symbolic address in step_cpu()";
    }

    Expression * data_in = NULL;
    if (m_cpu_write_enable) {
        // complete write
        m_cpu_write_handler[(address >> 12) & 0xF](*this, (address >> 12) & 0xF, (address & 0xFFF), m_cpu_data_out);
    } else {
        // complete read by setting data_in
        Expression * buf = m_cpu_read_handler[(address >> 12) & 0xF](*this, (address >> 12) & 0xF, (address & 0xFFF) );
        if (buf == NULL) {
            // bogus read, give all ones
            data_in = m.mk_byte(0xFF);
        } else {
            data_in = buf;
            m_cpu_last_read = buf;
        }
    }

    // now the CPU does something based on the read

    switch (m_cpu_state) {
    case CPU_Reset1:
    case CPU_Reset2:
    case CPU_Reset3:
    case CPU_Reset4:
    case CPU_Reset5:
    case CPU_Reset6:
    case CPU_Reset7:
    case CPU_Reset8:
        cpu_reset(); break;
    case CPU_Decode:
        // increment instruction pointer
        // check the opcode we just read
        // execute the first cycle of that opcode
        // TODO
    default:
        // TODO throw a proper exception, or do something better than this
        throw "Unhandled state" + std::to_string(m_cpu_state);
    }
}
