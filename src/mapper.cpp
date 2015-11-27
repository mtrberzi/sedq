#include "mapper.h"

Mapper::Mapper(uint8_t ines_flags) : m_ines_flags(ines_flags){}

Mapper::~Mapper(){}

bool Mapper::load(Context & ctx) { return false; }
void Mapper::reset(Context & ctx){}
void Mapper::unload(Context & ctx){}
void Mapper::cpu_cycle(Context & ctx){}
void Mapper::ppu_cycle(Context & ctx){}

void Mapper::set_PRG_ROM_4(Context & ctx, int bank, int val) {
    ctx.get_cpu_PRG_pointer()[bank] =  ctx.get_cpu_PRG_ROM()[val & ctx.get_prg_mask_rom()];
    ctx.get_cpu_readable()[bank] = true;
    ctx.get_cpu_writable()[bank] = false;
}

void Mapper::set_PRG_ROM_8(Context & ctx, int bank, int val) {
    val <<= 1;
    set_PRG_ROM_4(ctx, bank+0, val+0);
    set_PRG_ROM_4(ctx, bank+1, val+1);
}

void Mapper::set_PRG_ROM_16(Context & ctx, int bank, int val) {
    val <<= 2;
    set_PRG_ROM_4(ctx, bank+0, val+0);
    set_PRG_ROM_4(ctx, bank+1, val+1);
    set_PRG_ROM_4(ctx, bank+2, val+2);
    set_PRG_ROM_4(ctx, bank+3, val+3);
}

void Mapper::set_PRG_ROM_32(Context & ctx, int bank, int val) {
    val <<= 3;
    set_PRG_ROM_4(ctx, bank+0, val+0);
    set_PRG_ROM_4(ctx, bank+1, val+1);
    set_PRG_ROM_4(ctx, bank+2, val+2);
    set_PRG_ROM_4(ctx, bank+3, val+3);
    set_PRG_ROM_4(ctx, bank+4, val+4);
    set_PRG_ROM_4(ctx, bank+5, val+5);
    set_PRG_ROM_4(ctx, bank+6, val+6);
    set_PRG_ROM_4(ctx, bank+7, val+7);
}

Mapper * get_mapper(unsigned int mapper_id, uint8_t ines_flags) {
    switch (mapper_id) {
    case 0:
        return new Mapper000(ines_flags);
    default:
        throw "unknown mapper ID " + std::to_string(mapper_id);
    }
}
