#ifndef _AST_MANAGER_H_
#define _AST_MANAGER_H_

#include <cstdint>
#include <string>
#include <vector>
#include "expression.h"
#include "model.h"

enum ESolverStatus {
    SAT,
    UNSAT,
    UNKNOWN,
    ERROR
};

class ASTManager {
public:
    ASTManager() : m_varID(0) {}
    virtual ~ASTManager() {}

    // ground terms
    virtual Expression * mk_byte(uint8_t val) = 0;
    virtual Expression * mk_halfword(uint16_t val) = 0;
    virtual Expression * mk_var(std::string name, unsigned int nBits) = 0;
    Expression * mk_var(unsigned int nBits); // generate anonymous uniquely-named variable
    virtual Expression * mk_int(int32_t val) = 0;
    virtual Expression * mk_bool(bool val) = 0;

    // boolean terms
    virtual Expression * mk_and(Expression * arg0, Expression * arg1) = 0;
    virtual Expression * mk_or(Expression * arg0, Expression * arg1) = 0;
    virtual Expression * mk_not(Expression * arg) = 0;
    virtual Expression * mk_eq(Expression * arg0, Expression * arg1) = 0;
    virtual Expression * mk_assert(Expression * arg) = 0;

    // bitvector terms
    virtual Expression * mk_bv_and(Expression * arg0, Expression * arg1) = 0;
    virtual Expression * mk_bv_or(Expression * arg0, Expression * arg1) = 0;
    virtual Expression * mk_bv_xor(Expression * arg0, Expression * arg1) = 0;
    virtual Expression * mk_bv_not(Expression * arg) = 0;

    virtual Expression * mk_bv_neg(Expression * arg) = 0;
    virtual Expression * mk_bv_add(Expression * arg0, Expression * arg1) = 0;
    virtual Expression * mk_bv_sub(Expression * arg0, Expression * arg1) = 0;
    virtual Expression * mk_bv_mul(Expression * arg0, Expression * arg1) = 0;

    virtual Expression * mk_bv_concat(Expression * arg0, Expression * arg1) = 0;
    virtual Expression * mk_bv_extract(Expression * bv, Expression * hi, Expression * lo) = 0;

    virtual Expression * mk_bv_left_shift(Expression * bv, Expression * shiftamt) = 0;
    virtual Expression * mk_bv_logical_right_shift(Expression * bv, Expression * shiftamt) = 0;

    virtual Expression * mk_bv_unsigned_less_than(Expression * arg0, Expression * arg1) = 0;
    virtual Expression * mk_bv_unsigned_less_than_or_equal(Expression * arg0, Expression * arg1) = 0;
    virtual Expression * mk_bv_unsigned_greater_than(Expression * arg0, Expression * arg1) = 0;
    virtual Expression * mk_bv_unsigned_greater_than_or_equal(Expression * arg0, Expression * arg1) = 0;

    virtual Expression * mk_bv_signed_less_than(Expression * arg0, Expression * arg1) = 0;
    virtual Expression * mk_bv_signed_less_than_or_equal(Expression * arg0, Expression * arg1) = 0;
    virtual Expression * mk_bv_signed_greater_than(Expression * arg0, Expression * arg1) = 0;
    virtual Expression * mk_bv_signed_greater_than_or_equal(Expression * arg0, Expression * arg1) = 0;

    virtual ESolverStatus call_solver(std::vector<Expression*> & assertions, Model ** model) = 0;

protected:
    uint64_t m_varID; // variable ID counter
    std::string get_unique_variable_name();
};

class ASTManager_SMT2 : public ASTManager {
public:
    ASTManager_SMT2() {}
    virtual ~ASTManager_SMT2() {}

    Expression * mk_byte(uint8_t val);
    Expression * mk_halfword(uint16_t val);
    Expression * mk_var(std::string name, unsigned int nBits);
    Expression * mk_int(int32_t val);
    Expression * mk_bool(bool val);

    // boolean terms
    Expression * mk_and(Expression * arg0, Expression * arg1);
    Expression * mk_or(Expression * arg0, Expression * arg1);
    Expression * mk_not(Expression * arg);
    Expression * mk_eq(Expression * arg0, Expression * arg1);
    Expression * mk_assert(Expression * arg);

    // bitvector terms
    Expression * mk_bv_and(Expression * arg0, Expression * arg1);
    Expression * mk_bv_or(Expression * arg0, Expression * arg1);
    Expression * mk_bv_xor(Expression * arg0, Expression * arg1);
    Expression * mk_bv_not(Expression * arg);

    Expression * mk_bv_neg(Expression * arg);
    Expression * mk_bv_add(Expression * arg0, Expression * arg1);
    Expression * mk_bv_sub(Expression * arg0, Expression * arg1);
    Expression * mk_bv_mul(Expression * arg0, Expression * arg1);

    Expression * mk_bv_concat(Expression * arg0, Expression * arg1);
    Expression * mk_bv_extract(Expression * bv, Expression * hi, Expression * lo);

    Expression * mk_bv_left_shift(Expression * bv, Expression * shiftamt);
    Expression * mk_bv_logical_right_shift(Expression * bv, Expression * shiftamt);

    Expression * mk_bv_unsigned_less_than(Expression * arg0, Expression * arg1);
    Expression * mk_bv_unsigned_less_than_or_equal(Expression * arg0, Expression * arg1);
    Expression * mk_bv_unsigned_greater_than(Expression * arg0, Expression * arg1);
    Expression * mk_bv_unsigned_greater_than_or_equal(Expression * arg0, Expression * arg1);

    Expression * mk_bv_signed_less_than(Expression * arg0, Expression * arg1);
    Expression * mk_bv_signed_less_than_or_equal(Expression * arg0, Expression * arg1);
    Expression * mk_bv_signed_greater_than(Expression * arg0, Expression * arg1);
    Expression * mk_bv_signed_greater_than_or_equal(Expression * arg0, Expression * arg1);

    ESolverStatus call_solver(std::vector<Expression*> & assertions, Model ** model);

protected:
    std::string get_var_decl(Expression * var);
};

#endif // _AST_MANAGER_H_
