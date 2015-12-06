#ifndef _MODEL_H_
#define _MODEL_H_

#include <cstdint>
#include <string>
#include <map>

class Model {
public:
    Model();

    uint32_t get_variable_value(std::string name);
    uint8_t get_variable_width(std::string name);

    void add_variable(std::string name, uint32_t value, uint8_t width);

protected:
    std::map<std::string, uint32_t> m_variable_values;
    std::map<std::string, uint8_t> m_variable_widths;
};

#endif // _MODEL_H_
