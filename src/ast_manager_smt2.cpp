#include "ast_manager.h"
#include "expression.h"
#include <cstdint>

class SMT2Expression : public Expression {
public:
    SMT2Expression() {}
    virtual ~SMT2Expression() {}

    virtual std::string to_string() const = 0;
};

class BitVectorVariable : public SMT2Expression {
public:
    BitVectorVariable(std::string name, unsigned int bits) : m_name(name), m_bits(bits) {
    }
    virtual ~BitVectorVariable() {}

    std::string to_string() const {
        return m_name;
    }

    bool is_concrete() { return false;}
    uint32_t get_value() { return 0; }

    unsigned int get_width() {
        return m_bits;
    }
protected:
    std::string m_name;
    unsigned int m_bits;
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
protected:
    uint8_t m_val;
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
protected:
    std::string m_op;
    SMT2Expression * m_arg0;
    SMT2Expression * m_arg1;
};

Expression * ASTManager_SMT2::mk_byte(uint8_t val) {
    return new ByteConstant(val);
}

Expression * ASTManager_SMT2::mk_var(std::string name, unsigned int nBits) {
    return new BitVectorVariable(name, nBits);
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
