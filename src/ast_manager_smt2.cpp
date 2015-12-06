#include "ast_manager.h"
#include "expression.h"
#include <cstdint>
#include "trace.h"
#include <set>
#include <map>

static inline uint32_t get_bitmask(uint32_t nBits) {
    if (nBits >= 32) {
        return 0xFFFFFFFF;
    } else {
        return (1 << nBits) - 1;
    }
}

class SMT2Expression : public Expression {
public:
    SMT2Expression() {}
    virtual ~SMT2Expression() {}

    virtual std::string to_string() const = 0;
};

class BitVectorVariable : public SMT2Expression {
public:
    BitVectorVariable(std::string name, uint8_t bits) : m_name(name), m_bits(bits) {
    }
    virtual ~BitVectorVariable() {}

    std::string to_string() const {
        return m_name;
    }

    bool is_concrete() { return false;}
    uint32_t get_value() { return 0; }
    uint8_t get_width() { return m_bits; }
protected:
    std::string m_name;
    uint8_t m_bits;
};

class BooleanConstant : public SMT2Expression {
public:
    BooleanConstant(bool val) : m_val(val) {
    }
    virtual ~BooleanConstant() {}

    std::string to_string() const {
        if (m_val) {
            return "true";
        } else {
            return "false";
        }
    }

    bool is_concrete() { return true; }
    uint32_t get_value() { if (m_val) return 1 ; else return 0; }
    uint8_t get_width() { return 1; }
protected:
    bool m_val;
};

class ByteConstant : public SMT2Expression {
public:
    ByteConstant(uint8_t val) : m_val(val) {
    }
    virtual ~ByteConstant() {}

    std::string to_string() const {
        // we want to generate a constant of the form #bNNNNNNNN where each N is either 0 or 1
        std::string val = "#b";
        for(int i = 7; i >= 0; --i) {
            if ((m_val & (1 << i)) != 0) {
                val += "1";
            } else {
                val += "0";
            }
        }
        return val;
    }

    bool is_concrete() { return true; }
    uint32_t get_value() { return ((uint32_t)m_val) & 0x000000FF; }
    uint8_t get_width() { return 8; }
protected:
    uint8_t m_val;
};

class HalfwordConstant : public SMT2Expression {
public:
    HalfwordConstant(uint16_t val) : m_val(val) {
    }
    virtual ~HalfwordConstant() {}

    std::string to_string() const {
        // we want to generate a constant of the form #bNNNNNNNN where each N is either 0 or 1
        std::string val = "#b";
        for(int i = 15; i >= 0; --i) {
            if ((m_val & (1 << i)) != 0) {
                val += "1";
            } else {
                val += "0";
            }
        }
        return val;
    }

    bool is_concrete() { return true; }
    uint32_t get_value() { return ((uint32_t)m_val) & 0x0000FFFF; }
    uint8_t get_width() { return 16; }
protected:
    uint16_t m_val;
};

class IntegerConstant : public SMT2Expression {
public:
    IntegerConstant(int32_t val) : m_val(val) {
    }
    virtual ~IntegerConstant() {}

    std::string to_string() const {
        return std::to_string(m_val);
    }

    bool is_concrete() { return true; }
    uint32_t get_value() { return m_val; }
    uint8_t get_width() { return 32; }
protected:
    int32_t m_val;
};

class UnaryOp : public SMT2Expression {
public:
    UnaryOp(std::string oper, SMT2Expression * arg0) : m_op(oper), m_arg(arg0) {
    }
    virtual ~UnaryOp() {}

    std::string to_string() const {
        std::string str = "(";
        str += m_op;
        str += " ";
        str += m_arg->to_string();
        str += ")";
        return str;
    }

    bool is_concrete() { return false; }
    uint32_t get_value() { return 0; }
    uint8_t get_width() { return 0; }
protected:
    std::string m_op;
    SMT2Expression * m_arg;
};

class BinaryOp : public SMT2Expression {
public:
    BinaryOp(std::string oper, SMT2Expression * arg0, SMT2Expression * arg1) : m_op(oper), m_arg0(arg0), m_arg1(arg1) {
    }
    virtual ~BinaryOp() {}

    std::string to_string() const {
        std::string str = "(";
        str += m_op;
        str += " ";
        str += m_arg0->to_string();
        str += " ";
        str += m_arg1->to_string();
        str += ")";
        return str;
    }

    bool is_concrete() { return false; }
    uint32_t get_value() { return 0; }
    uint8_t get_width() { return 0; }
protected:
    std::string m_op;
    SMT2Expression * m_arg0;
    SMT2Expression * m_arg1;
};

class ExtractOp : public SMT2Expression {
public:
    ExtractOp(SMT2Expression * bv, SMT2Expression * hi, SMT2Expression * lo) : m_bv(bv), m_hi(hi), m_lo(lo) {
    }
    virtual ~ExtractOp(){}

    std::string to_string() const {
        std::string str = "((_ extract ";
        str += m_hi->to_string();
        str += " ";
        str += m_lo->to_string();
        str += ") ";
        str += m_bv->to_string();
        str += ")";
        return str;
    }

    bool is_concrete() { return false; }
    uint32_t get_value() { return 0; }
    uint8_t get_width() { return 0; }
protected:
    SMT2Expression * m_bv;
    SMT2Expression * m_hi;
    SMT2Expression * m_lo;
};

Expression * ASTManager_SMT2::mk_byte(uint8_t val) {
    return new ByteConstant(val);
}

Expression * ASTManager_SMT2::mk_halfword(uint16_t val) {
    return new HalfwordConstant(val);
}

Expression * ASTManager_SMT2::mk_var(std::string name, unsigned int nBits) {
    return new BitVectorVariable(name, nBits);
}

Expression * ASTManager_SMT2::mk_int(int32_t val) {
    return new IntegerConstant(val);
}

Expression * ASTManager_SMT2::mk_bool(bool val) {
    return new BooleanConstant(val);
}

Expression * ASTManager_SMT2::mk_and(Expression * arg0, Expression * arg1) {
    // we assume that this is well-sorted
    if (arg0->is_concrete() && arg1->is_concrete()) {
        uint32_t val0 = arg0->get_value() & get_bitmask(arg0->get_width());
        uint32_t val1 = arg1->get_value() & get_bitmask(arg1->get_width());
        return new BooleanConstant(val0 && val1);
    } else {
        return new BinaryOp("and", (SMT2Expression*)arg0, (SMT2Expression*)arg1);
    }
}

Expression * ASTManager_SMT2::mk_or(Expression * arg0, Expression * arg1) {
    // we assume that this is well-sorted
    if (arg0->is_concrete() && arg1->is_concrete()) {
        uint32_t val0 = arg0->get_value() & get_bitmask(arg0->get_width());
        uint32_t val1 = arg1->get_value() & get_bitmask(arg1->get_width());
        return new BooleanConstant(val0 || val1);
    } else {
        return new BinaryOp("=", (SMT2Expression*)arg0, (SMT2Expression*)arg1);
    }
}

Expression * ASTManager_SMT2::mk_not(Expression * arg) {
    // we really, really assume that this is well-sorted
    if (arg->is_concrete()) {
        return new BooleanConstant(arg->get_value() == 0);
    } else {
        return new UnaryOp("not", (SMT2Expression*)arg);
    }
}

Expression * ASTManager_SMT2::mk_eq(Expression * arg0, Expression * arg1) {
    // we assume that this is well-sorted
    if (arg0->is_concrete() && arg1->is_concrete()) {
        uint32_t val0 = arg0->get_value() & get_bitmask(arg0->get_width());
        uint32_t val1 = arg1->get_value() & get_bitmask(arg1->get_width());
        return new BooleanConstant(val0 == val1);
    } else {
        return new BinaryOp("=", (SMT2Expression*)arg0, (SMT2Expression*)arg1);
    }
}

Expression * ASTManager_SMT2::mk_assert(Expression * arg) {
    return new UnaryOp("assert", (SMT2Expression*)arg);
}

// bitvector terms

Expression * ASTManager_SMT2::mk_bv_and(Expression * arg0, Expression * arg1) {
    if (arg0->is_concrete() && arg1->is_concrete()) {
        uint32_t val0 = arg0->get_value();
        uint32_t val1 = arg1->get_value();
        if (arg0->get_width() == 8 && arg1->get_width() == 8) {
            return new ByteConstant( (uint8_t) ((val0 & val1) & get_bitmask(8)));
        } else if (arg0->get_width() == 16 && arg1->get_width() == 16) {
            return new HalfwordConstant( (uint16_t) ((val0 & val1) & get_bitmask(16)));
        }
    }
    // fall through
    return new BinaryOp("bvand", (SMT2Expression*)arg0, (SMT2Expression*)arg1);
}

Expression * ASTManager_SMT2::mk_bv_or(Expression * arg0, Expression * arg1) {
    if (arg0->is_concrete() && arg1->is_concrete()) {
        uint32_t val0 = arg0->get_value();
        uint32_t val1 = arg1->get_value();
        if (arg0->get_width() == 8 && arg1->get_width() == 8) {
            return new ByteConstant( (uint8_t) ((val0 | val1) & get_bitmask(8)));
        } else if (arg0->get_width() == 16 && arg1->get_width() == 16) {
            return new HalfwordConstant( (uint16_t) ((val0 | val1) & get_bitmask(16)));
        }
    }
    // fall through
    return new BinaryOp("bvor", (SMT2Expression*)arg0, (SMT2Expression*)arg1);
}

Expression * ASTManager_SMT2::mk_bv_xor(Expression * arg0, Expression * arg1) {
    if (arg0->is_concrete() && arg1->is_concrete()) {
        uint32_t val0 = arg0->get_value();
        uint32_t val1 = arg1->get_value();
        if (arg0->get_width() == 8 && arg1->get_width() == 8) {
            return new ByteConstant( (uint8_t) ((val0 ^ val1) & get_bitmask(8)));
        } else if (arg0->get_width() == 16 && arg1->get_width() == 16) {
            return new HalfwordConstant( (uint16_t) ((val0 ^ val1) & get_bitmask(16)));
        }
    }
    // fall through
    return new BinaryOp("bvxor", (SMT2Expression*)arg0, (SMT2Expression*)arg1);
}

Expression * ASTManager_SMT2::mk_bv_not(Expression * arg) {
    if (arg->is_concrete()) {
        uint32_t val = arg->get_value();
        if (arg->get_width() == 8) {
            return new ByteConstant((~val) & get_bitmask(8));
        } else if (arg->get_width() == 16) {
            return new HalfwordConstant((~val) & get_bitmask(16));
        }
    }
    // fall through
    return new UnaryOp("bvnot", (SMT2Expression*)arg);
}

Expression * ASTManager_SMT2::mk_bv_neg(Expression * arg) {
    if (arg->is_concrete()) {
        uint32_t val = arg->get_value();
        if (arg->get_width() == 8) {
            return new ByteConstant((-val) & get_bitmask(8));
        } else if (arg->get_width() == 16) {
            return new HalfwordConstant((-val) & get_bitmask(16));
        }
    }
    // fall through
    return new UnaryOp("bvnot", (SMT2Expression*)arg);
}

Expression * ASTManager_SMT2::mk_bv_add(Expression * arg0, Expression * arg1) {
    if (arg0->is_concrete() && arg1->is_concrete()) {
        uint32_t val0 = arg0->get_value();
        uint32_t val1 = arg1->get_value();
        if (arg0->get_width() == 8 && arg1->get_width() == 8) {
            return new ByteConstant( (uint8_t) ((val0 + val1) & get_bitmask(8)));
        } else if (arg0->get_width() == 16 && arg1->get_width() == 16) {
            return new HalfwordConstant( (uint16_t) ((val0 + val1) & get_bitmask(16)));
        }
    }
    // fall through
    return new BinaryOp("bvadd", (SMT2Expression*)arg0, (SMT2Expression*)arg1);
}

Expression * ASTManager_SMT2::mk_bv_sub(Expression * arg0, Expression * arg1) {
    if (arg0->is_concrete() && arg1->is_concrete()) {
        uint32_t val0 = arg0->get_value();
        uint32_t val1 = arg1->get_value();
        if (arg0->get_width() == 8 && arg1->get_width() == 8) {
            return new ByteConstant( (uint8_t) ((val0 - val1) & get_bitmask(8)));
        } else if (arg0->get_width() == 16 && arg1->get_width() == 16) {
            return new HalfwordConstant( (uint16_t) ((val0 - val1) & get_bitmask(16)));
        }
    }
    // fall through
    return new BinaryOp("bvsub", (SMT2Expression*)arg0, (SMT2Expression*)arg1);
}

Expression * ASTManager_SMT2::mk_bv_mul(Expression * arg0, Expression * arg1) {
    if (arg0->is_concrete() && arg1->is_concrete()) {
        uint32_t val0 = arg0->get_value();
        uint32_t val1 = arg1->get_value();
        if (arg0->get_width() == 8 && arg1->get_width() == 8) {
            return new ByteConstant( (uint8_t) ((val0 * val1) & get_bitmask(8)));
        } else if (arg0->get_width() == 16 && arg1->get_width() == 16) {
            return new HalfwordConstant( (uint16_t) ((val0 * val1) & get_bitmask(16)));
        }
    }
    // fall through
    return new BinaryOp("bvmul", (SMT2Expression*)arg0, (SMT2Expression*)arg1);
}

// TODO potentially create a better constant type in order to perform concat and extract concretely too

Expression * ASTManager_SMT2::mk_bv_concat(Expression * arg0, Expression * arg1) {
    if (arg0->is_concrete() && arg1->is_concrete()) {
        uint32_t val0 = arg0->get_value();
        uint32_t val1 = arg1->get_value();
        if (arg0->get_width() == 8 && arg1->get_width() == 8) {
            return new HalfwordConstant((val0 << 8 | val1) & get_bitmask(16));
        }
    }
    return new BinaryOp("concat", (SMT2Expression*)arg0, (SMT2Expression*)arg1);
}

Expression * ASTManager_SMT2::mk_bv_extract(Expression * bv, Expression * hi, Expression * lo) {
    if (bv->is_concrete() && hi->is_concrete() && lo->is_concrete()) {
        uint32_t bv_val = bv->get_value();
        int32_t high_bit = (int32_t)hi->get_value();
        int32_t low_bit = (int32_t)lo->get_value();
        if (high_bit - low_bit + 1 == 8) {
            // start by masking out everything above the highest bit
            uint32_t mask = get_bitmask(high_bit + 1);
            bv_val &= mask;
            // then shift down to clear out everything below the lowest bit
            bv_val >>= low_bit;
            return new ByteConstant(bv_val);
        }
    }
    return new ExtractOp((SMT2Expression*)bv, (SMT2Expression*)hi, (SMT2Expression*)lo);
}

Expression * ASTManager_SMT2::mk_bv_left_shift(Expression * bv, Expression * shiftamt) {
    if (bv->is_concrete() && shiftamt->is_concrete()) {
        uint32_t bv_val = bv->get_value();
        uint32_t shiftamt_val = shiftamt->get_value();
        if (bv->get_width() == 8 && shiftamt->get_width() == 8) {
            return new ByteConstant( (uint8_t) ((bv_val << shiftamt_val) & get_bitmask(8)));
        } else if (bv->get_width() == 16 && shiftamt->get_width() == 16) {
            return new HalfwordConstant( (uint16_t) ((bv_val << shiftamt_val) & get_bitmask(16)));
        }
    }
    // fall through
    return new BinaryOp("bvshl", (SMT2Expression*)bv, (SMT2Expression*)shiftamt);
}

Expression * ASTManager_SMT2::mk_bv_logical_right_shift(Expression * bv, Expression * shiftamt) {
    if (bv->is_concrete() && shiftamt->is_concrete()) {
        uint32_t bv_val = bv->get_value();
        uint32_t shiftamt_val = shiftamt->get_value();
        if (bv->get_width() == 8 && shiftamt->get_width() == 8) {
            return new ByteConstant( (uint8_t) ((bv_val >> shiftamt_val) & get_bitmask(8)));
        } else if (bv->get_width() == 16 && shiftamt->get_width() == 16) {
            return new HalfwordConstant( (uint16_t) ((bv_val >> shiftamt_val) & get_bitmask(16)));
        }
    }
    // fall through
    return new BinaryOp("bvlshr", (SMT2Expression*)bv, (SMT2Expression*)shiftamt);
}

Expression * ASTManager_SMT2::mk_bv_unsigned_less_than(Expression * arg0, Expression * arg1) {
    if (arg0->is_concrete() && arg1->is_concrete()) {
        uint32_t val0 = arg0->get_value() & get_bitmask(arg0->get_width());
        uint32_t val1 = arg1->get_value() & get_bitmask(arg1->get_width());
        return new BooleanConstant(val0 < val1);
    } else {
        return new BinaryOp("bvult", (SMT2Expression*)arg0, (SMT2Expression*)arg1);
    }
}

Expression * ASTManager_SMT2::mk_bv_unsigned_less_than_or_equal(Expression * arg0, Expression * arg1) {
    if (arg0->is_concrete() && arg1->is_concrete()) {
        uint32_t val0 = arg0->get_value() & get_bitmask(arg0->get_width());
        uint32_t val1 = arg1->get_value() & get_bitmask(arg1->get_width());
        return new BooleanConstant(val0 <= val1);
    } else {
        return new BinaryOp("bvule", (SMT2Expression*)arg0, (SMT2Expression*)arg1);
    }
}

Expression * ASTManager_SMT2::mk_bv_unsigned_greater_than(Expression * arg0, Expression * arg1) {
    if (arg0->is_concrete() && arg1->is_concrete()) {
        uint32_t val0 = arg0->get_value() & get_bitmask(arg0->get_width());
        uint32_t val1 = arg1->get_value() & get_bitmask(arg1->get_width());
        return new BooleanConstant(val0 > val1);
    } else {
        return new BinaryOp("bvugt", (SMT2Expression*)arg0, (SMT2Expression*)arg1);
    }
}

Expression * ASTManager_SMT2::mk_bv_unsigned_greater_than_or_equal(Expression * arg0, Expression * arg1) {
    if (arg0->is_concrete() && arg1->is_concrete()) {
        uint32_t val0 = arg0->get_value() & get_bitmask(arg0->get_width());
        uint32_t val1 = arg1->get_value() & get_bitmask(arg1->get_width());
        return new BooleanConstant(val0 >= val1);
    } else {
        return new BinaryOp("bvuge", (SMT2Expression*)arg0, (SMT2Expression*)arg1);
    }
}

Expression * ASTManager_SMT2::mk_bv_signed_less_than(Expression * arg0, Expression * arg1) {
    if (arg0->is_concrete() && arg1->is_concrete()) {
        if (arg0->get_width() == 8 && arg1->get_width() == 8) {
            int8_t val0 = (int8_t)(arg0->get_value()& get_bitmask(8));
            int8_t val1 = (int8_t)(arg1->get_value()& get_bitmask(8));
            return new BooleanConstant(val0 < val1);
        } else if (arg0->get_width() == 16 && arg1->get_width() == 16) {
            int16_t val0 = (int16_t)(arg0->get_value()& get_bitmask(16));
            int16_t val1 = (int16_t)(arg1->get_value()& get_bitmask(16));
            return new BooleanConstant(val0 < val1);
        }
    }
    // fall through
    return new BinaryOp("bvslt", (SMT2Expression*)arg0, (SMT2Expression*)arg1);
}

Expression * ASTManager_SMT2::mk_bv_signed_less_than_or_equal(Expression * arg0, Expression * arg1) {
    if (arg0->is_concrete() && arg1->is_concrete()) {
        if (arg0->get_width() == 8 && arg1->get_width() == 8) {
            int8_t val0 = (int8_t)(arg0->get_value()& get_bitmask(8));
            int8_t val1 = (int8_t)(arg1->get_value()& get_bitmask(8));
            return new BooleanConstant(val0 <= val1);
        } else if (arg0->get_width() == 16 && arg1->get_width() == 16) {
            int16_t val0 = (int16_t)(arg0->get_value()& get_bitmask(16));
            int16_t val1 = (int16_t)(arg1->get_value()& get_bitmask(16));
            return new BooleanConstant(val0 <= val1);
        }
    }
    // fall through
    return new BinaryOp("bvsle", (SMT2Expression*)arg0, (SMT2Expression*)arg1);
}

Expression * ASTManager_SMT2::mk_bv_signed_greater_than(Expression * arg0, Expression * arg1) {
    if (arg0->is_concrete() && arg1->is_concrete()) {
        if (arg0->get_width() == 8 && arg1->get_width() == 8) {
            int8_t val0 = (int8_t)(arg0->get_value()& get_bitmask(8));
            int8_t val1 = (int8_t)(arg1->get_value()& get_bitmask(8));
            return new BooleanConstant(val0 > val1);
        } else if (arg0->get_width() == 16 && arg1->get_width() == 16) {
            int16_t val0 = (int16_t)(arg0->get_value()& get_bitmask(16));
            int16_t val1 = (int16_t)(arg1->get_value()& get_bitmask(16));
            return new BooleanConstant(val0 > val1);
        }
    }
    // fall through
    return new BinaryOp("bvsgt", (SMT2Expression*)arg0, (SMT2Expression*)arg1);
}

Expression * ASTManager_SMT2::mk_bv_signed_greater_than_or_equal(Expression * arg0, Expression * arg1) {
    if (arg0->is_concrete() && arg1->is_concrete()) {
        if (arg0->get_width() == 8 && arg1->get_width() == 8) {
            int8_t val0 = (int8_t)(arg0->get_value()& get_bitmask(8));
            int8_t val1 = (int8_t)(arg1->get_value()& get_bitmask(8));
            return new BooleanConstant(val0 >= val1);
        } else if (arg0->get_width() == 16 && arg1->get_width() == 16) {
            int16_t val0 = (int16_t)(arg0->get_value()& get_bitmask(16));
            int16_t val1 = (int16_t)(arg1->get_value()& get_bitmask(16));
            return new BooleanConstant(val0 >= val1);
        }
    }
    // fall through
    return new BinaryOp("bvsge", (SMT2Expression*)arg0, (SMT2Expression*)arg1);
}

ESolverStatus ASTManager_SMT2::call_solver(Expression ** assertions, unsigned int nAssertions, Model * model) {
    SMT2Expression** smt2assertions = (SMT2Expression**)assertions;
    std::string instance;

    // start with the usual boilerplate
    instance = "(set-logic QF_BV)\n";

    // now declare all variables
    std::set<std::string> unique_variable_names;
    std::map<std::string, BitVectorVariable*> variables;

    for (unsigned int i = 0; i < nAssertions; ++i) {
        // TODO collect variables and declare them
    }

    // turn every expression into an assertion
    for (unsigned int i = 0; i < nAssertions; ++i) {
        instance += ((SMT2Expression*)mk_assert(smt2assertions[i]))->to_string();
        instance += "\n";
    }

    // here we assume that STP is being used...
    instance += "(check-sat)\n(exit)\n";

    TRACE("solver", tout << instance << std::endl;);

    // TODO call the solver
    throw "don't yet know how to invoke the solver";

}

/*
 * Get the SMT2 representation of a variable declaration.
 * Since STP doesn't know what (declare-const) is, we instead
 * use the slightly more verbose (declare-fun), which we hope
 * everyone supports (at least STP and Z3 do).
 */
std::string ASTManager_SMT2::get_var_decl(Expression * var) {
    // TODO throw an exception if 'var' is not actually a BitVectorVariable
    BitVectorVariable * bv_var = (BitVectorVariable*)var;
    std::string decl = "(declare-fun ";
    decl += bv_var->to_string();
    decl += " () (_ BitVec ";
    decl += std::to_string(bv_var->get_width());
    decl += "))";
    return decl;
}
