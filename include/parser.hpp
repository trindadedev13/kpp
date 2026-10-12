/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (C) 2026 Aquiles Trindade
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstddef>
#include <format>
#include <iostream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "ast.hpp"
#include "lexer.hpp"
#include "token.hpp"

namespace kpp
{

struct parser
{
    parser(const file_info &fi)
        : m_fi{fi},
          m_pos{0}
    {
    }

    ast::program parse();

private:
    ast::expr_ref<ast::expr> parse_expr(void);
    ast::expr_ref<ast::expr> parse_assignment(void);
    ast::expr_ref<ast::expr> parse_additive(void);
    ast::expr_ref<ast::expr> parse_multiplicative(void);
    ast::expr_ref<ast::expr> parse_postfix(void);
    ast::expr_ref<ast::expr> parse_primary(void);

    struct __fndecl
    {
        std::string name;
        std::vector<std::string> v_params;
        std::string ret_type;
    };

    __fndecl parse_function_decl(void);
    ast::expr_ref<ast::function_expr> parse_function(void);

    ast::expr_ref<ast::call_expr> parse_call(ast::expr_ref<ast::expr>);
    ast::expr_ref<ast::var_decl_expr> parse_var_decl(void);
    ast::expr_ref<ast::directive_expr> parse_directive(const token &);
    ast::expr_ref<ast::return_expr> parse_return(void);
    ast::expr_ref<ast::block_expr> parse_block(void);
    ast::expr_ref<ast::aggregate_expr> parse_aggregate(const std::string &);
    ast::expr_ref<ast::struct_aggregate_expr> parse_struct(void);
    ast::expr_ref<ast::namespace_aggregate_expr> parse_namespace(void);

    ast::expr_ref<ast::extern_directive_expr> parse_extern_directive(void);
    ast::expr_ref<ast::import_directive_expr> parse_import_directive(void);

    const token *advance();
    const token *consume(token::kind);

    inline const token *current(std::size_t offset = 0) const
    {
        std::size_t pos = m_pos + offset;
        if (pos >= m_fi.tokens.size())
            return nullptr;
        return &m_fi.tokens.at(pos);
    }

    inline const token *previous(void) const
    {
        if (m_pos > 0)
            return &m_fi.tokens.at(m_pos - 1);
        return nullptr;
    }

    template <typename... Args>
    [[noreturn]] void error(const token &tk, std::format_string<Args...> fmt, Args &&...args)
    {
        std::cerr << std::format("error[{}:{}:{}]: {}",
                                 m_fi.filename,
                                 tk.get_line(),
                                 tk.get_column(),
                                 std::format(fmt, std::forward<Args>(args)...))
                  << std::endl;
        std::exit(1);
    }

private:
    file_info m_fi;
    std::size_t m_pos;
};

} // namespace kpp
