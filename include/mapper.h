#ifndef _MAPPER_H_
#define _MAPPER_H_

#include "context.h"

class Mapper {
public:
    Mapper(uint8_t ines_flags);
    virtual ~Mapper();

    virtual bool load(Context & ctx);
    virtual void reset(Context & ctx);
    virtual void unload(Context & ctx);
    virtual void cpu_cycle(Context & ctx);
    virtual void ppu_cycle(Context & ctx);
protected:
    uint8_t m_ines_flags;

    void set_PRG_ROM_4(Context & ctx, int bank, int val);
    void set_PRG_ROM_8(Context & ctx, int bank, int val);
    void set_PRG_ROM_16(Context & ctx, int bank, int val);
    void set_PRG_ROM_32(Context & ctx, int bank, int val);
};

Mapper * get_mapper(unsigned int mapper_id, uint8_t ines_flags);

class Mapper000 : public Mapper {
public:
    Mapper000(uint8_t ines_flags);
    virtual ~Mapper000();

    bool load(Context & ctx);
    void reset(Context & ctx);
};

#endif // _MAPPER_H_
