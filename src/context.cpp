#include <cstdlib>
#include <cstdint>
#include "context.h"
#include "mapper.h"
#include "ast_manager.h"
#include "context_scheduler.h"
#include "trace.h"

static Expression * CPU_ReadRAM(Context & ctx, uint8_t bank, uint16_t addr) {
    return ctx.cpu_read_ram(addr & 0x07FF);
}

static void CPU_WriteRAM(Context & ctx, uint8_t bank, uint16_t addr, Expression * val) {
    ctx.cpu_write_ram(addr & 0x07FF, val);
}

Expression * Context::cpu_read_ram(uint16_t addr) {
    if (m_cpu_ram == NULL) {
        // check copy-on-write cache, then ask parent
        std::map<uint16_t, Expression*>::iterator target = m_cpu_ram_copyonwrite.find(addr);
        if (target == m_cpu_ram_copyonwrite.end()) {
            return m_parent_context->cpu_read_ram(addr);
        } else {
            return target->second;
        }
    } else {
        return m_cpu_ram[addr];
    }
}

void Context::cpu_write_ram(uint16_t addr, Expression * value) {
    if (m_cpu_ram == NULL) {
        // only write to copy-on-write cache
        m_cpu_ram_copyonwrite[addr] = value;
    } else {
        m_cpu_ram[addr] = value;
    }
}

static Expression * PPU_IntRead(Context & ctx, uint8_t bank, uint16_t addr) {
    // TODO PPU_IntRead
    return NULL;
}

static void PPU_IntWrite(Context & ctx, uint8_t bank, uint16_t addr, Expression * val) {
    // TODO PPU_IntWrite
}

static Expression * APU_IntRead(Context & ctx, uint8_t bank, uint16_t addr) {
    ASTManager & m = ctx.get_manager();
    Expression * result = m.mk_byte(0xFF);
    switch (addr) {
    /*
     * In general, controller reads are done like this:
     *  result = CPU::LastRead & 0xC0
     *  result |= Controllers::Port#->Read() & 0x19
     *  result |= Controllers::PortExp->Read#() & 0x1F
     *  return result
     */
    case 0x016:
        // controller port 1
        result = m.mk_bv_and(ctx.get_cpu_last_read(), m.mk_byte(0xC0));
        result = m.mk_bv_or(result, m.mk_bv_and(ctx.controller_read1(), m.mk_byte(0x19)));
        break;
    case 0x017:
        // controller port 2
        result = m.mk_bv_and(ctx.get_cpu_last_read(), m.mk_byte(0xC0));
        result = m.mk_bv_or(result, m.mk_bv_and(ctx.controller_read2(), m.mk_byte(0x19)));
        break;
    }
    return result;
}

static void APU_IntWrite(Context & ctx, uint8_t bank, uint16_t addr, Expression * val) {
    switch (addr) {
    case 0x016:
        ctx.controller_write(val);
        break;
    }
}

static Expression * CPU_ReadPRG(Context & ctx, uint8_t bank, uint16_t addr) {
    TRACE("read_prg", tout << "bank = " << std::to_string(bank) << ", addr = " << std::to_string(addr) << std::endl;);
    if (ctx.get_cpu_readable()[bank]) {
        return ctx.get_cpu_PRG_pointer()[bank][addr];
    } else {
        return NULL;
    }
}

static void CPU_WritePRG(Context & ctx, uint8_t bank, uint16_t addr, Expression * val) {
    if (ctx.get_cpu_writable()[bank]) {
        ctx.get_cpu_PRG_pointer()[bank][addr] = val;
    }
}

Context::Context(ASTManager & m, ContextScheduler & sch)
: m(m), sch(sch), m_parent_context(NULL), m_has_forked(false),
  m_step_count(0), m_next_device(EDevice::Device_CPU), m_frame_number(0),
  m_mapper(NULL),
  m_mapper_prg_size_ram(0), m_mapper_prg_size_rom(0),
  m_mapper_chr_size_ram(0), m_mapper_chr_size_rom(0),
  m_PRG_ROM(NULL), m_CHR_ROM(NULL),
  // CPU
  m_cpu_cycle_count(0), m_cpu_pcm_cycles(0), m_cpu_current_opcode(0),
  m_cpu_state(ECPUState::CPU_Reset1), m_cpu_memory_phase(true),
  m_cpu_want_nmi(false), m_cpu_want_irq(false),
  m_cpu_addressing_mode_state(CPU_AM_NON), m_cpu_addressing_mode_cycle(0), m_cpu_execute_cycle(0),
  m_cpu_A(m.mk_byte(0)), m_cpu_X(m.mk_byte(0)), m_cpu_Y(m.mk_byte(0)), m_cpu_SP(m.mk_byte(0)), m_cpu_PC(m.mk_halfword(0)),
  m_cpu_FC(m.mk_byte(0)), m_cpu_FZ(m.mk_byte(0)), m_cpu_FI(m.mk_byte(0)),
  m_cpu_FD(m.mk_byte(0)), m_cpu_FV(m.mk_byte(0)), m_cpu_FN(m.mk_byte(0)),
  m_cpu_last_read(m.mk_byte(0)), m_cpu_calc_addr(NULL), m_cpu_branch_offset(NULL),
  // Controllers
  m_controller1_bits(NULL), m_controller1_bit_ptr(0), m_controller1_strobe(false), m_controller1_seqno(0),
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
    m_cpu_ram = new Expression*[0x800];
    for (unsigned int i = 0; i < 0x800; ++i) {
        m_cpu_ram[i] = m.mk_byte(0);
    }
}

Context::Context(ASTManager & m, Context * parent)
: m(m), sch(parent->get_scheduler()), m_parent_context(parent), m_has_forked(false),
  m_step_count(parent->m_step_count), m_next_device(parent->m_next_device), m_frame_number(parent->m_frame_number),
  m_mapper(parent->m_mapper),
  m_mapper_prg_size_ram(parent->m_mapper_prg_size_ram), m_mapper_prg_size_rom(parent->m_mapper_prg_size_rom),
  m_mapper_chr_size_ram(parent->m_mapper_chr_size_ram), m_mapper_chr_size_rom(parent->m_mapper_chr_size_rom),
  m_PRG_ROM(parent->m_PRG_ROM), m_CHR_ROM(parent->m_CHR_ROM),
  // CPU
  m_cpu_cycle_count(parent->m_cpu_cycle_count), m_cpu_pcm_cycles(parent->m_cpu_pcm_cycles), m_cpu_current_opcode(parent->m_cpu_current_opcode),
  m_cpu_state(parent->m_cpu_state), m_cpu_memory_phase(parent->m_cpu_memory_phase),
  m_cpu_want_nmi(parent->m_cpu_want_nmi), m_cpu_want_irq(parent->m_cpu_want_irq),
  m_cpu_addressing_mode_state(parent->m_cpu_addressing_mode_state), m_cpu_addressing_mode_cycle(parent->m_cpu_addressing_mode_cycle),
  m_cpu_execute_cycle(parent->m_cpu_execute_cycle),
  m_cpu_A(parent->m_cpu_A), m_cpu_X(parent->m_cpu_X), m_cpu_Y(parent->m_cpu_Y), m_cpu_SP(parent->m_cpu_SP), m_cpu_PC(parent->m_cpu_PC),
  m_cpu_FC(parent->m_cpu_FC), m_cpu_FZ(parent->m_cpu_FZ), m_cpu_FI(parent->m_cpu_FI),
  m_cpu_FD(parent->m_cpu_FD), m_cpu_FV(parent->m_cpu_FV), m_cpu_FN(parent->m_cpu_FN),
  m_cpu_last_read(parent->m_cpu_last_read), m_cpu_calc_addr(parent->m_cpu_calc_addr), m_cpu_branch_offset(parent->m_cpu_branch_offset),
  // Controllers
  m_controller1_bits(parent->m_controller1_bits), m_controller1_bit_ptr(parent->m_controller1_bit_ptr),
  m_controller1_strobe(parent->m_controller1_strobe), m_controller1_seqno(parent->m_controller1_seqno),
  // Address Bus
  m_cpu_address(parent->m_cpu_address), m_cpu_write_enable(parent->m_cpu_write_enable), m_cpu_data_out(parent->m_cpu_data_out)
{
    // *** CPU initialization ***
    // CPU read/write handlers
    for (unsigned int i = 0; i < 0x10; ++i) {
        m_cpu_read_handler[i] = parent->m_cpu_read_handler[i];
        m_cpu_write_handler[i] = parent->m_cpu_write_handler[i];
        m_cpu_readable[i] = parent->m_cpu_readable[i];
        m_cpu_writable[i] = parent->m_cpu_writable[i];
        m_cpu_prg_pointer[i] = parent->m_cpu_prg_pointer[i];
    }

    m_cpu_ram = NULL;
}

Context::~Context() {
}

void Context::load_iNES(std::istream & in) {
    int i;
    char Header[16];
    in.read(Header, 16);
    // check iNES header signature
    if (Header[0] != 'N' || Header[1] != 'E' || Header[2] != 'S' || Header[3] != '\x1A') {
        throw "iNES header signature not found";
    }
    if ((Header[7] & 0x0C) == 0x04) {
        throw "header is corrupted by \"DiskDude!\"";
    }
    if ((Header[7] & 0x0C) == 0x0C) {
        throw "header format not recognized";
    }

    uint8_t ines_PRGsize = Header[4];
    uint8_t ines_CHRsize = Header[5];
    TRACE("ines", tout << "PRG size = " << (ines_PRGsize<<4) << "KB, CHR size = " << std::to_string((ines_CHRsize<<3)) << "KB" << std::endl;);
    uint8_t ines_mapper_num = ((Header[6] & 0xF0) >> 4) | (Header[7] & 0xF0);
    TRACE("ines", tout << "mapper #" << std::to_string(ines_mapper_num) << std::endl;);
    uint8_t ines_flags = (Header[6] & 0x0F) | ((Header[7] & 0x0F) << 4);

    bool ines2 = false;
    if ((Header[7] & 0x0C) == 0x08) {
        ines2 = true;
        throw "NES 2.0 ROM image detected; not yet supported...";
    } else {
        for (unsigned int i = 8; i < 0x10; ++i) {
            if (Header[i] != 0) {
                throw "unrecognized data found in header";
            }
        }
    }
    if (ines_flags & 0x04) {
        throw "trained ROMs are not supported";
    }

    m_mapper_prg_size_rom = ines_PRGsize * 0x4;
    m_mapper_chr_size_rom = ines_CHRsize * 0x8;

    m_PRG_ROM = (Expression***)malloc(sizeof(Expression**) * MAX_PRG_ROM_SIZE);
    for (unsigned int i = 0; i < MAX_PRG_ROM_SIZE; ++i) {
        m_PRG_ROM[i] = (Expression**)malloc(sizeof(Expression*) * 0x1000);
    }

    m_CHR_ROM = (Expression***)malloc(sizeof(Expression**) * MAX_CHR_ROM_SIZE);
    for (unsigned int i = 0; i < MAX_CHR_ROM_SIZE; ++i) {
        m_CHR_ROM[i] = (Expression**)malloc(sizeof(Expression*) * 0x400);
    }

    char * PRG_ROM_buffer = new char[m_mapper_prg_size_rom * 0x4000];
    in.read(PRG_ROM_buffer, m_mapper_prg_size_rom * 0x4000);
    char * CHR_ROM_buffer = new char[m_mapper_chr_size_rom * 0x2000];
    in.read(CHR_ROM_buffer, m_mapper_chr_size_rom * 0x2000);

    for (unsigned int bank = 0; bank < m_mapper_prg_size_rom * 4; ++bank) {
        for (unsigned int pos = 0; pos < 0x1000; ++pos) {
            unsigned int index = bank * 0x1000 + pos;
            char val = PRG_ROM_buffer[index];
            m_PRG_ROM[bank][pos] = get_manager().mk_byte(val);
        }
    }

    // TODO initialize CHR ROM

    /*
    for (unsigned int i = 0; i < m_mapper_prg_size_rom * 0x4000; ++i) {
        m_PRG_ROM[i] = get_manager().mk_byte(PRG_ROM_buffer[i]);
    }

    for (unsigned int i = 0; i < m_mapper_chr_size_rom * 0x2000; ++i) {
        m_CHR_ROM[i] = get_manager().mk_byte(CHR_ROM_buffer[i]);
    }
    */

    uint8_t ines_PRGram_size;
    uint8_t ines_CHRram_size;

    if (ines2) {
        // TODO PRG/CHR RAM stuff
    } else {
        // default to 64KB of PRG RAM and 32KB of CHR RAM
        ines_PRGram_size = 0x10;
        ines_CHRram_size = 0x20;
    }

    // load mapper
    m_mapper = get_mapper(ines_mapper_num, ines_flags);
    m_mapper->load(*this);
    m_mapper->reset(*this);

    // TODO extra stuff for playchoice-10 and vs. unisystem roms to autoselect palette

    delete[] PRG_ROM_buffer;
    delete[] CHR_ROM_buffer;
}


ASTManager & Context::get_manager() {
    return m;
}

ContextScheduler & Context::get_scheduler() {
    return sch;
}

int Context::get_priority() const {
    // TODO do something here
    return 0;
}

bool Context::has_forked() const {
    return m_has_forked;
}

uint64_t Context::get_cpu_cycle_count() {
    return m_cpu_cycle_count;
}

// TODO front-half read() and write() force a switch to the next peripheral
// see MemGet() and MemSet()

Expression * Context::get_cpu_last_read() {
    return m_cpu_last_read;
}

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
    TRACE("step", tout << "step " << std::to_string(m_step_count) << std::endl;);
    switch (m_next_device) {
    case Device_CPU:
    {
        TRACE("step", tout << "stepping CPU" << std::endl;);
        step_cpu();
    } break;
    }
    m_step_count += 1;
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

Expression * Context::get_cpu_FN() {
    if (m_cpu_FN == NULL) {
        m_cpu_FN = m_parent_context->get_cpu_FN();
    }
    return m_cpu_FN;
}

Expression * Context::get_cpu_FV() {
    if (m_cpu_FV == NULL) {
        m_cpu_FV = m_parent_context->get_cpu_FV();
    }
    return m_cpu_FV;
}

Expression * Context::get_cpu_FD() {
    if (m_cpu_FD == NULL) {
        m_cpu_FD = m_parent_context->get_cpu_FD();
    }
    return m_cpu_FD;
}

Expression * Context::get_cpu_FI() {
    if (m_cpu_FI == NULL) {
        m_cpu_FI = m_parent_context->get_cpu_FI();
    }
    return m_cpu_FI;
}

Expression * Context::get_cpu_FZ() {
    if (m_cpu_FZ == NULL) {
        m_cpu_FZ = m_parent_context->get_cpu_FZ();
    }
    return m_cpu_FZ;
}

Expression * Context::get_cpu_FC() {
    if (m_cpu_FC == NULL) {
        m_cpu_FC = m_parent_context->get_cpu_FC();
    }
    return m_cpu_FC;
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

Expression *** Context::get_cpu_PRG_ROM() {
    if (m_parent_context != NULL) {
        m_PRG_ROM = m_parent_context->get_cpu_PRG_ROM();
    }
    return m_PRG_ROM;
}

static uint32_t getMask (unsigned int maxval){
    uint32_t result = 0;
    while (maxval > 0)
    {
        result = (result << 1) | 1;
        maxval >>= 1;
    }
    return result;
}

uint32_t Context::get_prg_mask_rom() {
 //     NES::PRGMaskROM = NES::getMask(NES::PRGSizeROM - 1) & MAX_PRGROM_MASK;
    uint32_t mask = getMask(m_mapper_prg_size_rom - 1);
    return mask & (MAX_PRG_ROM_SIZE - 1);
}

void Context::cpu_reset() {
    TRACE("cpu", tout << "In reset sequence..." << std::endl;);
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
        instruction_fetch();
        break;
    }
}

void Context::instruction_fetch() {
    cpu_read(get_cpu_PC());
    m_cpu_state = CPU_Decode;
}

// returns: true iff the addressing mode allows the current instruction to start executing immediately
bool Context::decode_addressing_mode() {
	switch (m_cpu_current_opcode) {
	case 0x00: case 0x80: case 0xA0: case 0xC0: case 0xE0: case 0x82: case 0xA2: case 0xC2: case 0xE2:
	case 0x09: case 0x29: case 0x49: case 0x69: case 0x89: case 0xA9: case 0xC9: case 0xE9:
	case 0x0B: case 0x2B: case 0x4B: case 0x6B: case 0x8B: case 0xAB: case 0xCB: case 0xEB:
		m_cpu_addressing_mode_state = CPU_AM_IMM; break;
	case 0x0C: case 0x2C: case 0x4C: case 0x6C: case 0x8C: case 0xAC: case 0xCC: case 0xEC:
	case 0x0E: case 0x2E: case 0x4E: case 0x6E: case 0x8E: case 0xAE: case 0xCE: case 0xEE:
	case 0x0D: case 0x2D: case 0x4D: case 0x6D: case 0x8D: case 0xAD: case 0xCD: case 0xED:
	case 0x0F: case 0x2F: case 0x4F: case 0x6F: case 0x8F: case 0xAF: case 0xCF: case 0xEF:
	    m_cpu_addressing_mode_state = CPU_AM_ABS; break;
	case 0x10: case 0x30: case 0x50: case 0x70: case 0x90: case 0xB0: case 0xD0: case 0xF0:
	    m_cpu_addressing_mode_state = CPU_AM_REL; break;
	case 0x1C: case 0x3C: case 0x5C: case 0x7C: case 0xBC: case 0xDC: case 0xFC:
	case 0x1D: case 0x3D: case 0x5D: case 0x7D: case 0xBD: case 0xDD: case 0xFD:
	    m_cpu_addressing_mode_state = CPU_AM_ABX; break;
	default:
		throw "failed to decode addressing mode";
	}

	// check whether the addressing mode can complete without accessing memory
	switch (m_cpu_addressing_mode_state) {
	case CPU_AM_IMM:
	case CPU_AM_NON:
		return true;
	default:
		return false;
	}

}

void Context::increment_PC() {
    m_cpu_PC = m.mk_bv_add(get_cpu_PC(), m.mk_halfword(0x0001));
}

// sets FC = (test >= 0)
void Context::cpu_set_FC(Expression * test) {
    m_cpu_FC = m.mk_bv_signed_greater_than_or_equal(test, m.mk_byte(0));
}

// sets FN = (test >> 7) == 0x01
void Context::cpu_set_FN(Expression * test) {
    m_cpu_FN = m.mk_eq(m.mk_bv_logical_right_shift(test, m.mk_byte(7)), m.mk_byte(1));
}

// sets FZ = (test == 0)
void Context::cpu_set_FZ(Expression * test) {
    m_cpu_FZ = m.mk_eq(test, m.mk_byte(0));
}

void Context::cpu_addressing_mode_cycle() {
    switch (m_cpu_addressing_mode_state) {
    case CPU_AM_IMM:
        /*
         * CalcAddr = PC
         * PC++
         */
        switch (m_cpu_addressing_mode_cycle) {
        case 0:
            m_cpu_calc_addr = get_cpu_PC();
            increment_PC();
            // done
            m_cpu_state = CPU_Execute;
            break;
        }
        break;
    case CPU_AM_ABS:
        /*
         * CalcAddr[7:0] = MemGetCode(PC++)
         * CalcAddr[15:8] = MemGetCode(PC++)
         */
        switch (m_cpu_addressing_mode_cycle) {
        case 0:
            cpu_read(get_cpu_PC());
            increment_PC();
            break;
        case 1:
            m_cpu_calc_addr = m.mk_bv_concat(m.mk_byte(0), m_cpu_last_read);
            cpu_read(get_cpu_PC());
            increment_PC();
            break;
        case 2:
            m_cpu_calc_addr = m.mk_bv_concat(m_cpu_last_read, m.mk_bv_extract(m_cpu_calc_addr, m.mk_int(7), m.mk_int(0)));
            // done
            m_cpu_state = CPU_Execute;
            break;
        }
        break;
    case CPU_AM_REL:
        /*
         * BranchOffset = MemGetCode(PC++)
         */
        switch (m_cpu_addressing_mode_cycle) {
        case 0:
            cpu_read(get_cpu_PC());
            increment_PC();
            break;
        case 1:
            m_cpu_branch_offset = m_cpu_last_read;
            // done
            m_cpu_state = CPU_Execute;
            break;
        }
        break;
    case CPU_AM_ABX:
        /*
        CalcAddrL = MemGetCode(PC++);
        CalcAddrH = MemGetCode(PC++);
        bool inc = (CalcAddrL + X) >= 0x100;
        CalcAddrL += X;
        if (inc)
        {
                MemGet(CalcAddr);
                CalcAddrH++;
        }
        */
        switch (m_cpu_addressing_mode_cycle) {
        case 0:
            cpu_read(get_cpu_PC());
            increment_PC();
            break;
        case 1:
            m_cpu_calc_addr = m.mk_bv_concat(m.mk_byte(0), m_cpu_last_read);
            cpu_read(get_cpu_PC());
            increment_PC();
            break;
        case 2:
        {
            Expression * CalcAddrL = m.mk_bv_extract(m_cpu_calc_addr, m.mk_int(7), m.mk_int(0));
            if (CalcAddrL->is_concrete() && get_cpu_X()->is_concrete()) {
                uint32_t val = CalcAddrL->get_value() + get_cpu_X()->get_value();
                // set CalcAddr = [LastRead | CalcAddrL + X]
                m_cpu_calc_addr = m.mk_bv_concat(m_cpu_last_read, m.mk_bv_add(CalcAddrL, get_cpu_X()));
                if (val >= 0x100) {
                    // extra cycle required -- waste time reading from this bogus address
                    cpu_read(m_cpu_calc_addr);
                } else {
                    // done -- no extra cycle
                    m_cpu_state = CPU_Execute;
                }
            } else {
                throw "oops, symbolic CalcAddr or symbolic X register in ABX addressing mode";
            }
        }
            break;
        case 3:
        {
            // this is the extra cycle
            // throw away the read value, increment CalcAddrH, and we're done
            Expression * CalcAddrH = m.mk_bv_extract(m_cpu_calc_addr, m.mk_int(15), m.mk_int(8));
            Expression * CalcAddrL = m.mk_bv_extract(m_cpu_calc_addr, m.mk_int(7), m.mk_int(0));
            m_cpu_calc_addr = m.mk_bv_concat(m.mk_bv_add(CalcAddrH, m.mk_byte(0x01)), CalcAddrL);
            // finally done
            m_cpu_state = CPU_Execute;
        }
            break;
        }
        break;
    default:
        TRACE("cpu", tout << "unhandled addressing mode" << std::endl;);
        throw "oops, unhandled addressing mode";
    }
    m_cpu_addressing_mode_cycle += 1;
}

void Context::cpu_branch(Expression * condition) {
    if (condition->is_concrete()) {
        if (condition->get_value() != 0) {
            switch (m_cpu_execute_cycle) {
            case 0:
                // TODO special interrupt ignoring "bug"
                cpu_read(get_cpu_PC());
                break;
            case 1:
            {
                // TODO re-enable interrupts here, or something?
                /*
                bool inc = (PCL + BranchOffset) >= 0x100;
                PCL += BranchOffset;
                if (BranchOffset & 0x80)
                {
                        if (!inc)
                            {
                                    MemGet(PC);
                                PCH--;
                        }
                }
                else
                    {
                        if (inc)
                            {
                                MemGet(PC);
                                PCH++;
                        }
                }
        `       */
                if (m_cpu_branch_offset->is_concrete()) {
                    uint32_t val = ( get_cpu_PC()->get_value() & 0x00FF ) + m_cpu_branch_offset->get_value();
                    bool inc = (val >= 0x100);
                    // PC[7:0] = PC[7:0] + BranchOffset
                    Expression * PCH = m.mk_bv_extract(get_cpu_PC(), m.mk_int(15), m.mk_int(8));
                    Expression * PCL = m.mk_bv_extract(get_cpu_PC(), m.mk_int(7), m.mk_int(0));
                    m_cpu_PC = m.mk_bv_concat(PCH, m.mk_bv_add(PCL, m_cpu_branch_offset));
                    // decide whether an extra cycle is needed due to page crossing
                    if (m_cpu_branch_offset->get_value() & 0x80) {
                        if (!inc) {
                            // extra cycle
                            cpu_read(m_cpu_PC);
                        } else {
                            // no extra cycle
                            instruction_fetch();
                        }
                    } else {
                        if (inc) {
                            // extra cycle
                            cpu_read(m_cpu_PC);
                        } else {
                            // no extra cycle
                            instruction_fetch();
                        }
                    }
                } else {
                    throw "oops, symbolic branch offset";
                }
                break;
            } // case 1
            case 2:
            {
                // we can assume m_cpu_branch_offset is concrete
                // all we have to do is adjust PCH in the appropriate direction
                Expression * PCH = m.mk_bv_extract(get_cpu_PC(), m.mk_int(15), m.mk_int(8));
                Expression * PCL = m.mk_bv_extract(get_cpu_PC(), m.mk_int(7), m.mk_int(0));
                if (m_cpu_branch_offset->get_value() & 0x80) {
                    // PCH --
                    m_cpu_PC = m.mk_bv_concat(m.mk_bv_sub(PCH, m.mk_byte(1)), PCL);
                } else {
                    // PCH ++
                    m_cpu_PC = m.mk_bv_concat(m.mk_bv_add(PCH, m.mk_byte(1)), PCL);
                }
                instruction_fetch();
                break;
            } // case 2
            }
        }
    } else {
        TRACE("cpu", tout << "symbolic branch: " << condition->to_string() << std::endl;);
        throw "oops, symbolic branch";
    }
}

void Context::cpu_execute() {
    switch (m_cpu_current_opcode) {
    case 0x21: case 0x31: case 0x29: case 0x39: case 0x25: case 0x35: case 0x2D: case 0x3D:
        // AND
        /*
         * A = A & MemGet(CalcAddr)
         * FZ = (A == 0)
         * FN = (A >> 7) == 0x01;
         */
        switch (m_cpu_execute_cycle) {
        case 0:
            cpu_read(m_cpu_calc_addr);
            break;
        case 1:
            m_cpu_A = m.mk_bv_and(get_cpu_A(), m_cpu_last_read);
            cpu_set_FZ(m_cpu_A);
            cpu_set_FN(m_cpu_A);
            instruction_fetch();
            break;
        }
        break;
    case 0x90:
        // BCC
        cpu_branch(m.mk_not(get_cpu_FC()));
        break;
    case 0xB0:
        // BCS
        cpu_branch(get_cpu_FC());
        break;
    case 0xF0:
        // BEQ
        cpu_branch(get_cpu_FZ());
        break;
    case 0x30:
        // BMI
        cpu_branch(get_cpu_FN());
        break;
    case 0xD0:
        // BNE
        cpu_branch(m.mk_not(get_cpu_FZ()));
        break;
    case 0x10:
        // BPL
        cpu_branch(m.mk_not(get_cpu_FN()));
        break;
    case 0x50:
        // BVC
        cpu_branch(m.mk_not(get_cpu_FV()));
        break;
    case 0x70:
        // BVS
        cpu_branch(get_cpu_FV());
        break;
    case 0xC1: case 0xD1: case 0xC9: case 0xD9: case 0xC5: case 0xD5: case 0xCD: case 0xDD:
        // CMP
        /*
         * result = A - MemGet(CalcAddr)
         * FC = (result >= 0)
         * FZ = (result == 0)
         * FN = (result >> 7) == 0x01
         */
        switch (m_cpu_execute_cycle) {
        case 0:
            cpu_read(m_cpu_calc_addr);
            break;
        case 1:
            Expression * result = m.mk_bv_sub(get_cpu_A(), m_cpu_last_read);
            cpu_set_FC(result);
            cpu_set_FN(result);
            cpu_set_FZ(result);
            instruction_fetch();
            break;
        }
        break;
    case 0xA1: case 0xB1: case 0xA9: case 0xB9: case 0xA5: case 0xB5: case 0xAD: case 0xBD:
        // LDA
        /*
         * A = MemGet(CalcAddr)
         * FZ = (A == 0)
         * FN = (A >> 7) == 0x01
         */
        switch (m_cpu_execute_cycle) {
        case 0:
            cpu_read(m_cpu_calc_addr);
            break;
        case 1:
            m_cpu_A = m_cpu_last_read;
            cpu_set_FN(m_cpu_A);
            cpu_set_FZ(m_cpu_A);
            instruction_fetch();
            break;
        }
        break;
    case 0xA2: case 0xA6: case 0xB6: case 0xAE: case 0xBE:
        // LDX
        /*
         * X = MemGet(CalcAddr)
         * FZ = (X == 0)
         * FN = (X >> 7) == 0x01
         */
        switch (m_cpu_execute_cycle) {
        case 0:
            cpu_read(m_cpu_calc_addr);
            break;
        case 1:
            m_cpu_X = m_cpu_last_read;
            cpu_set_FN(m_cpu_X);
            cpu_set_FZ(m_cpu_X);
            instruction_fetch();
            break;
        }
        break;
    case 0xA0: case 0xA4: case 0xB4: case 0xAC: case 0xBC:
        // LDY
        /*
         * Y = MemGet(CalcAddr)
         * FZ = (Y == 0)
         * FN = (Y >> 7) == 0x01
         */
        switch (m_cpu_execute_cycle) {
        case 0:
            cpu_read(m_cpu_calc_addr);
            break;
        case 1:
            m_cpu_Y = m_cpu_last_read;
            cpu_set_FN(m_cpu_Y);
            cpu_set_FZ(m_cpu_Y);
            instruction_fetch();
            break;
        }
        break;
    case 0x81: case 0x91: case 0x99: case 0x85: case 0x95: case 0x8D: case 0x9D:
        // STA
        /*
         * MemSet(CalcAddr, A)
         */
        switch (m_cpu_execute_cycle) {
        case 0:
            cpu_write(m_cpu_calc_addr, get_cpu_A());
            break;
        case 1:
            instruction_fetch();
            break;
        }
        break;
    case 0x86: case 0x96: case 0x8E:
        // STX
        switch (m_cpu_execute_cycle) {
        case 0:
            cpu_write(m_cpu_calc_addr, get_cpu_X());
            break;
        case 1:
            instruction_fetch();
            break;
        }
        break;
    case 0x84: case 0x94: case 0x8C:
        // STY
        switch (m_cpu_execute_cycle) {
        case 0:
            cpu_write(m_cpu_calc_addr, get_cpu_Y());
            break;
        case 1:
            instruction_fetch();
            break;
        }
        break;
    default:
        TRACE("cpu", tout << "unimplemented instruction " << std::to_string(m_cpu_current_opcode) << std::endl;);
        throw "oops, unimplemented instruction";
    }
    m_cpu_execute_cycle += 1;
}

void Context::step_cpu() {
    TRACE("cpu",
            if (m_cpu_state == CPU_Decode) {
                tout << "cpu state = Decode" << std::endl;
            } else if (m_cpu_state == CPU_AddressingMode) {
                tout << "cpu state = AddressingMode" << std::endl;
            } else if (m_cpu_state == CPU_Execute) {
                tout << "cpu state = Execute" << std::endl;
            } else {
                tout << "cpu state = " << std::to_string(m_cpu_state) << std::endl;
            }
            );

    // CPU steps always start by completing the memory access from the previous step,
    // unless we're starting a context that forked some time after this memory access
    // (which is probably most of the reason why we're forking at all).
    // We can't redo this operation because memory accesses can have side effects that we don't
    // want to repeat. Instead we set a checkpoint for the current context, and if we fork,
    // the new context can pick up this checkpoint and figure out to resume after the memory phase.
    uint16_t address;

    if (m_cpu_memory_phase) {
        // deal with the address right away
        if (get_cpu_address()->is_concrete()) {
            address = (uint16_t) (get_cpu_address()->get_value() & 0x0000FFFF);
            TRACE("cpu_memory", tout << "access memory at " << std::to_string(address) << std::endl;);
        } else {
            // oh no. symbolic address.
            // TODO as soon as I figure out how context forks will work
            throw "oops, symbolic address in step_cpu()";
        }

        if (m_cpu_write_enable) {
            // complete write
            m_cpu_write_handler[(address >> 12) & 0xF](*this, (address >> 12) & 0xF, (address & 0xFFF), m_cpu_data_out);
        } else {
            // complete read by setting data_in
            Expression * buf = m_cpu_read_handler[(address >> 12) & 0xF](*this, (address >> 12) & 0xF, (address & 0xFFF) );
            if (buf == NULL) {
                // bogus read, give all ones
                // data_in = m.mk_byte(0xFF);
                m_cpu_last_read = m.mk_byte(0xFF);
                TRACE("cpu_memory", tout << "read failed" << std::endl;);
            } else {
                // data_in = buf;
                m_cpu_last_read = buf;
                CTRACE("cpu_memory", m_cpu_last_read->is_concrete(), tout << "read value " << m_cpu_last_read->get_value() << std::endl;);
            }
        }
        m_cpu_memory_phase = false;
    } else {
        TRACE("cpu", tout << "skipping memory phase" << std::endl;);
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
        // check the opcode we just read
        if (m_cpu_last_read->is_concrete()) {
            // do this increment here so that we don't increment it twice if we fork
            increment_PC();
            m_cpu_current_opcode = (uint8_t)(m_cpu_last_read->get_value() & 0xFF);
            TRACE("cpu", tout << "opcode = " << std::to_string(m_cpu_current_opcode) << std::endl;);
            m_cpu_addressing_mode_cycle = 0;
            m_cpu_execute_cycle = 0;
            m_cpu_state = CPU_AddressingMode;
            // figure out which addressing mode we want
            bool can_start_instruction = decode_addressing_mode();
            if (can_start_instruction) {
                TRACE("cpu", tout << "addressing mode completes in zero cycles" << std::endl;);
                cpu_addressing_mode_cycle();
            	cpu_execute();
            } else {
            	cpu_addressing_mode_cycle();
            }
        } else {
            throw "oops, symbolic opcode in step_cpu()";
        }
        break;
    case CPU_AddressingMode:
    	cpu_addressing_mode_cycle();
    	// if the state just became CPU_Execute, follow through into the first cycle of the instruction
    	if (m_cpu_state == CPU_Execute) {
    	    m_cpu_execute_cycle = 0;
    	    cpu_execute();
    	}
    	break;
    case CPU_Execute:
        cpu_execute();
        // TODO if the state just became Decode, check for interrupts
        break;
    default:
        // TODO throw a proper exception, or do something better than this
        TRACE("err", tout << "Unhandled state " << std::to_string(m_cpu_state) << std::endl;);
        throw "oops, unhandled state";
    }

#define PRINT_FLAG(flag, name_set, name_unset) { Expression * tmp = flag; \
    if (tmp->is_concrete()) {if (tmp->get_value() == 1) tout << name_set << " " ; else tout << name_unset << " ";} \
    else {tout << name_set << "?";} }

    TRACE("cpu",
        tout << "registers at end of step:" << std::endl;
        tout << "A = " << get_cpu_A()->to_string() << std::endl;
        tout << "X = " << get_cpu_X()->to_string() << std::endl;
        tout << "Y = " << get_cpu_Y()->to_string() << std::endl;
        tout << "SP = " << get_cpu_SP()->to_string() << std::endl;
        tout << "PC = " << get_cpu_PC()->to_string() << std::endl;
        // P: N V . . D I Z C
        tout << "P = ";
        PRINT_FLAG(get_cpu_FN(), "N", "n");
        PRINT_FLAG(get_cpu_FV(), "V", "v");
        tout << ". ";
        tout << ". ";
        PRINT_FLAG(get_cpu_FD(), "D", "d");
        PRINT_FLAG(get_cpu_FI(), "I", "i");
        PRINT_FLAG(get_cpu_FZ(), "Z", "z");
        PRINT_FLAG(get_cpu_FC(), "C", "c");
        tout << std::endl;
        );
    m_cpu_memory_phase = true;
    m_cpu_cycle_count += 1;
}

std::vector<Expression*> & Context::get_controller1_inputs() {
    return m_controller1_inputs;
}

// TODO regenerating controller_bits causes extra vars to be generated -- try to eliminate these if they aren't used

void Context::controller_write(Expression * val) {
    TRACE("controller", tout << "write controllers" << std::endl;);
    if (val->is_concrete()) {
        bool strobe = val->get_value() & 1;
        if (m_controller1_strobe || strobe) {
            m_controller1_strobe = strobe;
            m_controller1_bits = controller_mk_var(1);
            m_controller1_bit_ptr = 0;
        }
    } else {
        throw "oops, symbolic value in controller_write()";
    }
}

// TODO allow playback of concrete controller inputs (this requires knowing when frames happen)

Expression * Context::controller_read1() {
    TRACE("controller", tout << "read controller 1" << std::endl;);
    Expression * result;
    if (m_controller1_strobe) {
        m_controller1_bits = controller_mk_var(1);
        m_controller1_bit_ptr = 0;
        result = m.mk_bv_and(m_controller1_bits, m.mk_byte(0x01));
    } else {
        if (m_controller1_bit_ptr < 8) {
            result = m.mk_bv_logical_right_shift(m.mk_bv_and(m_controller1_bits, m.mk_byte(1 << m_controller1_bit_ptr)),
                    m.mk_byte(m_controller1_bit_ptr));
            m_controller1_bit_ptr += 1;
        } else {
            result = m.mk_byte(1);
        }
    }
    return result;
}

Expression * Context::controller_read2() {
    // TODO
    return NULL;
}

Expression * Context::controller_mk_var(int controller_number) {
    std::string var_name = "controller" + std::to_string(controller_number) + "_frame" + std::to_string(m_frame_number);
    if (controller_number == 1) {
        var_name += "_" + std::to_string(m_controller1_seqno);
        m_controller1_seqno += 1;
    } else if (controller_number == 2) {
        // TODO seqno 2
    }
    Expression * var = m.mk_var(var_name, 8);
    m_controller1_inputs.push_back(var);
    return var;
}
