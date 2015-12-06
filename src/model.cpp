#include "model.h"

Model::Model() { }

uint32_t Model::get_variable_value(std::string name) {
    return m_variable_values[name];
}

uint8_t Model::get_variable_width(std::string name) {
    return m_variable_widths[name];
}

void Model::add_variable(std::string name, uint32_t value, uint8_t width) {
    m_variable_values[name] = value;
    m_variable_widths[name] = width;
}
