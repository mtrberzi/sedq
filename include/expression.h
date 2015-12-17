#ifndef _EXPRESSION_H_
#define _EXPRESSION_H_

#include <cstdint>
#include <string>

class Expression {
public:
    Expression() {}
    virtual ~Expression() {}

    virtual bool is_concrete() = 0;
    /*
     * Note that if !is_concrete(), the return value is undefined.
     */
    virtual uint32_t get_value() = 0;
    virtual uint8_t get_width() = 0;

    virtual std::string to_string() const = 0;
};

#endif // _EXPRESSION_H_
