// iNES mapper 0: NROM

#include "mapper.h"

Mapper000::Mapper000(uint8_t ines_flags) : Mapper(ines_flags) {}

Mapper000::~Mapper000(){}

bool Mapper000::load(Context & ctx) {
    // TODO iNES_SetSRAM()
    return true;
}

void Mapper000::reset(Context & ctx) {
    // TODO iNES_SetMirroring()

    set_PRG_ROM_32(ctx, 0x8, 0);

    // TODO CHR ROM
    // TODO PRG RAM
    /*
     * if (ROM->INES_CHRSize) {
     *   EMU->SetCHR_ROM8(0, 0)
     * } else {
     *   EMU->SetCHR_RAM8(0, 0)
     * }
     * if (ROM->INES_Flags & 0x02) {
     *   EMU->SetPRG_RAM8(0x6, 0)
     * }
     */

}
