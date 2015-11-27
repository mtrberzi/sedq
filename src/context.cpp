#include <cstdlib>
#include <cstdint>
#include "context.h"
#include "mapper.h"
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

Context::Context(ASTManager & m)
: m(m), m_parent_context(NULL), m_step_count(0), m_next_device(EDevice::Device_CPU),
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
        cpu_read(get_cpu_PC());
        m_cpu_state = CPU_Decode;
        break;
    }
}

void Context::step_cpu() {
    TRACE("cpu", tout << "cpu state = " << std::to_string(m_cpu_state) << std::endl;);

    // CPU steps always start by completing the memory access from the previous step
    uint16_t address;
    // deal with the address right away
    if (get_cpu_address()->is_concrete()) {
        address = (uint16_t) (get_cpu_address()->get_value() & 0x0000FFFF);
        TRACE("cpu_memory", tout << "access memory at " << std::to_string(address) << std::endl;);
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
            TRACE("cpu_memory", tout << "read failed" << std::endl;);
        } else {
            data_in = buf;
            m_cpu_last_read = buf;
            CTRACE("cpu_memory", data_in->is_concrete(), tout << "read value " << data_in->get_value() << std::endl;);
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
