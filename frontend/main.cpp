#include <iostream>
#include <strstream>
#include <cstdlib>
#include "ast_manager.h"
#include "context.h"
#include "context_scheduler.h"
#include "trace.h"

// test harness

static char * mk_ines_rom(uint8_t mapper, char * prg_rom, uint8_t prg_pages, char * chr_rom, uint8_t chr_pages) {
    // TODO other flag bits
    char * image = new char[16 + (16384 * prg_pages) + (8192 * chr_pages)];
    uint8_t flags6 = ((mapper << 4) & 0xF0) | 0x00;
    uint8_t flags7 = 0x00 | (mapper >> 4);
    // header
    image[0] = 'N';
    image[1] = 'E';
    image[2] = 'S';
    image[3] = '\x1A';
    image[4] = prg_pages;
    image[5] = chr_pages;
    image[6] = flags6;
    image[7] = flags7;
    for (unsigned int i = 8; i <= 15; ++i) {
        image[i] = 0;
    }
    // PRG
    for (unsigned int i = 16; i < 16 + (16384 * prg_pages); ++i) {
        image[i] = prg_rom[i-16];
    }
    // CHR
    for (unsigned int i = 16 + (16384 * prg_pages); i < 16 + (16384 * prg_pages) + (8192 * chr_pages); ++i) {
        image[i] = chr_rom[i - 16 - (16384 * prg_pages)];
    }

    return image;

}

int main(int argc, char *argv[]) {
    open_trace();

    // TODO read arguments

    ASTManager_SMT2 mgr;
    ContextScheduler scheduler;

    Context * initial_context = new Context(mgr, scheduler);
    scheduler.add_context(initial_context);

    // most of the following is a test harness for now

    // load a fake rom
    uint8_t prg_pages = 1;
    uint8_t chr_pages = 1;
    char prg_rom[16384 * prg_pages];
    // set reset vector = 0xC000 (start of PRG)
    prg_rom[0xFFFC - 0xC000] = 0x00;
    prg_rom[0xFFFD - 0xC000] = 0xC0;
    // LDA #12
    prg_rom[0xC000 - 0xC000] = 0xA9;
    prg_rom[0xC001 - 0xC000] = 12;
    // CMP #7
    prg_rom[0xC002 - 0xC000] = 0xC9;
    prg_rom[0xC003 - 0xC000] = 7;

    char chr_rom[8192 * chr_pages];

    char * image = mk_ines_rom(0, prg_rom, prg_pages, chr_rom, chr_pages);
    std::istrstream rom_input(image, 16 + (16384 * prg_pages) + (8192 * chr_pages));
    initial_context->load_iNES(rom_input);

    // set up stopping conditions
    scheduler.set_maximum_cpu_cycles(7 + 2 + 2);

    // run scheduler
    try {
        while (scheduler.have_contexts()) {
            scheduler.run_next_context();
        }
    } catch (const char * msg) {
        std::cerr << "exception: " << msg << std::endl;
    }

    delete image;
    delete initial_context;

    close_trace();
    return EXIT_SUCCESS;
}
