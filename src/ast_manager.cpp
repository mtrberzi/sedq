#include "ast_manager.h"

Expression * ASTManager::mk_var(unsigned int nBits) {
    return mk_var(get_unique_variable_name(), nBits);
}

std::string ASTManager::get_unique_variable_name() {
    std::string name = "v" + std::to_string(m_varID);
    m_varID += 1;
    return name;
}
