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

#include "parser.hpp"

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "ast.hpp"
#include "error.hpp"
#include "token.hpp"

namespace kpp
{

const token *parser::advance()
{
    if (m_pos >= m_fi.tokens.size())
        return nullptr;

    return &m_fi.tokens.at(m_pos++);
}

const token *parser::consume(token::kind kind)
{
    auto c = current();
    if (!c)
        return nullptr;

    if (c->get_kind() != kind)
    {
        return nullptr;
    }
    m_pos++;
    return c;
}

ast::program parser::parse()
{
    std::vector<ast::expr_ref<ast::expr>> nodes{};
    while (m_pos < m_fi.tokens.size())
    {
        auto node = parse_expr();
        if (!node)
            break;
        nodes.push_back(std::move(node));
    }
    return {std::move(nodes)};
}

ast::expr_ref<ast::expr> parser::parse_expr()
{
    return parse_assignment();
}

ast::expr_ref<ast::expr> parser::parse_assignment()
{
    auto left = parse_additive();
    if (auto tok = current(); tok && tok->get_kind() == token::kind::equals)
    {
        advance();
        auto right = parse_assignment();
        return ast::make_expr<ast::assignment_expr>(std::move(left), std::move(right));
    }
    return left;
}

ast::expr_ref<ast::expr> parser::parse_additive(void)
{
    auto left = parse_multiplicative();
    while (auto tok = current())
    {
        if (!tok || (tok->get_kind() != token::kind::plus && tok->get_kind() != token::kind::minus))
            break;

        auto op = tok->get_kind() == token::kind::plus ? ast::binary_expr::op_kind::add
                                                       : ast::binary_expr::op_kind::sub;
        advance();
        auto right = parse_multiplicative();
        left = ast::make_expr<ast::binary_expr>(std::move(left), std::move(right), op);
    }

    return left;
}

ast::expr_ref<ast::expr> parser::parse_multiplicative(void)
{
    auto left = parse_postfix();
    while (auto tok = current())
    {
        if (!tok || (tok->get_kind() != token::kind::star && tok->get_kind() != token::kind::slash))
            break;

        auto op = tok->get_kind() == token::kind::star ? ast::binary_expr::op_kind::mul
                                                       : ast::binary_expr::op_kind::div;
        advance();
        auto right = parse_postfix();
        left = ast::make_expr<ast::binary_expr>(std::move(left), std::move(right), op);
    }
    return left;
}

ast::expr_ref<ast::expr> parser::parse_postfix()
{
    auto expr = parse_primary();
    while (true)
    {
        auto tok = current();
        if (!tok)
            break;

        switch (tok->get_kind())
        {
        case token::kind::open_paren:
        {
            expr = parse_call(std::move(expr));
            break;
        }

        case token::kind::dot:
        {
            auto dot = advance();

            auto member = consume(token::kind::identifier);
            if (!member)
                error(*dot, "unexpected dot");

            expr = ast::make_expr<ast::member_access_expr>(std::move(expr), member->get_value());
            break;
        }

        case token::kind::colon:
        {
            advance();

            if (auto tok = current(); tok && tok->get_kind() == token::kind::colon)
            {
                advance();
                auto member = current();
                if (!member || (member->get_kind() != token::kind::identifier &&
                                member->get_kind() != token::kind::star))
                    error(*tok, "unexpected '::'");

                advance();
                expr = ast::make_expr<ast::namespace_access_expr>(std::move(expr),
                                                                  member->get_value());
                break;
            }
            error(*current(), "unexpected ':'");
        }
        default:
            return expr;
        }
    }
    return expr;
}

ast::expr_ref<ast::expr> parser::parse_primary()
{
    auto token = current();
    if (!token)
        panic("invalid token");

    // it's a block
    if (token->is_block_start())
    {
        advance();
        return parse_block();
    }

    switch (token->get_kind())
    {

    case token::kind::identifier:
    {
        auto id = advance();
        return ast::make_expr<ast::identifier_expr>(id->get_value());
    }

    case token::kind::directive:
    {
        auto name = advance();
        return parse_directive(*name);
    }

    // parse literals too
    case token::kind::bool_lit:
    {
        auto value_expr = advance();
        return ast::make_expr<ast::literal_expr<bool>>(value_expr->get_value() == "true" ? true
                                                                                         : false);
    }

    case token::kind::number_lit:
    {
        auto value_expr = advance();
        return ast::make_expr<ast::literal_expr<int>>(std::atoi(value_expr->get_value().c_str()));
    }

    case token::kind::string_lit:
    {
        auto value_expr = advance();
        return ast::make_expr<ast::literal_expr<std::string>>(value_expr->get_value());
    }

    case token::kind::keyword:
    {
        auto kw = current();
        if (kw->get_value() == "work")
            return parse_function();
        else if (kw->get_value() == "let")
            return parse_var_decl();
        else if (kw->get_value() == "return")
            return parse_return();
        else if (kw->get_value() == "struct")
            return parse_struct();
        else if (kw->get_value() == "namespace")
            return parse_namespace();
        else if (kw->get_value() == "null")
        {
            advance();
            return ast::make_expr<ast::literal_expr<std::nullptr_t>>(nullptr);
        }

        error(*kw, "unexpected keyword: {}", kw->get_value());
    }

    case token::kind::eof:
        break;
    default:
        error(*token, "unknown token: {}", *token);
    }
    return nullptr;
}

parser::__fndecl parser::parse_function_decl(void)
{
    consume(token::kind::keyword); // work
    auto name = consume(token::kind::identifier);

    // parse params
    std::vector<std::string> params{};
    consume(token::kind::open_paren);
    while (true)
    {
        auto next = current();
        if (next && next->get_kind() == token::kind::close_paren)
        {
            advance();
            break;
        }

        auto p_name = consume(token::kind::identifier);
        if (!p_name)
            error(*previous(), "expected identifier after {}", m_fi.tokens.at(m_pos - 1));

        params.push_back(p_name->get_value());

        next = current();
        if (next)
        {
            if (next->get_kind() == token::kind::comma)
                advance();
            else if (next->get_kind() != token::kind::close_paren)
                error(*next, "expected ',' or ')' after paramether");
        }
    }

    // just for visual typing, internally the type doesn't really matter
    std::string ret_type{"any"};
    auto next = current();
    if (next && next->get_kind() == token::kind::arrow)
    {
        advance();
        next = current();
        if (next && next->get_kind() == token::kind::identifier)
        {
            auto val = advance()->get_value();
            ret_type = val;
        }
    }

    return __fndecl{.name = name->get_value(), .v_params = params, .ret_type = ret_type};
}

ast::expr_ref<ast::function_expr> parser::parse_function()
{
    auto decl = parse_function_decl();

    auto next = current();
    if (!next || !next->is_block_start())
        error(*previous(), "expected a block after function declaration");

    advance();
    auto block = parse_block();
    if (!block)
        error(*next, "invalid block");

    return ast::make_expr<ast::function_expr>(
        decl.name, std::vector<std::string>(decl.v_params), std::move(block));
}

ast::expr_ref<ast::call_expr> parser::parse_call(ast::expr_ref<ast::expr> callee)
{
    auto next = consume(token::kind::open_paren);
    if (!next)
        error(*previous(), "expected '('");

    std::vector<ast::expr_ref<ast::expr>> args{};
    while (true)
    {
        next = current();
        if (next && next->get_kind() == token::kind::close_paren)
        {
            advance();
            break;
        }

        auto node = parse_expr();
        if (!node)
            error(*next, "invalid argument node, {}", current()->get_value());
        args.push_back(std::move(node));

        next = current();
        if (next)
        {
            if (next->get_kind() == token::kind::comma)
                advance();
            else if (next->get_kind() != token::kind::close_paren)
                error(*next, "expected ',' or ')' after argument");
        }
    }

/** disable because it conflicts with blocks after calls,
 * like:
 *  func()
 *  // *a block that doesn't make part of *func**
 *  do
 *    puts ("Hi")
 *  end
 *
 * Now, because of the code below, the block is part of the call
 * and we don't want it, a way to fix it, is keep tracking of new lines
 * but it requires a refactor that i don't want to do, as far for now.
 */
#if 0
    // now we need to parse a block after the call (if have)
    next = current();
    if (next && next->is_block_start())
    {
        std::println("344 {}", *next);
        auto start = advance();
        auto block = parse_block();
        if (!block)
            error(*start, "invalid node block");

        // the block arg, ALWAYW are/must be the last argument
        args.push_back(std::move(block));
    }
#endif

    return ast::make_expr<ast::call_expr>(std::move(callee), std::move(args));
}

ast::expr_ref<ast::var_decl_expr> parser::parse_var_decl()
{
    consume(token::kind::keyword); // let
    auto name_token = current();
    if (!name_token || (name_token->get_kind() != token::kind::identifier &&
                        name_token->get_kind() != token::kind::star))
        error(*name_token, "expected \"identifier\" or \"star\" after \"let\" keyword");

    // skip identifier or star
    advance();

    auto name = name_token->get_value();
    consume(token::kind::equals);
    auto value_expr = parse_expr();
    auto node = ast::make_expr<ast::var_decl_expr>(name, std::move(value_expr));
    return node;
}

ast::expr_ref<ast::directive_expr> parser::parse_directive(const token &name_token)
{
    auto name = name_token.get_value();
    if (name == "import")
        return parse_import_directive();
    if (name == "extern")
        return parse_extern_directive();
    return nullptr;
}

ast::expr_ref<ast::return_expr> parser::parse_return()
{
    consume(token::kind::keyword); // return
    auto value = parse_expr();
    return ast::make_expr<ast::return_expr>(std::move(value));
}

ast::expr_ref<ast::block_expr> parser::parse_block()
{
    std::vector<ast::expr_ref<ast::expr>> body;
    while (true)
    {
        auto next = current();
        if (!next)
            error(*previous(), "expected token found EOF.");

        if (next->is_block_end())
        {
            advance();
            break;
        }

        auto node = parse_expr();
        if (!node)
            error(*next, "invalid block body node");
        body.push_back(std::move(node));
    }
    return ast::make_expr<ast::block_expr>(std::move(body));
}

ast::expr_ref<ast::struct_aggregate_expr> parser::parse_struct()
{
    consume(token::kind::keyword); // struct
    auto name = consume(token::kind::identifier);
    if (!name)
        error(*previous(), "expected a identifier after struct keyword");

    auto next = current();
    if (!next)
        error(*previous(), "expected block found EOF.");

    if (!next->is_block_start())
        error(*previous(), "expected block start, got {}", next->get_kind());

    advance();
    auto block = parse_block();
    if (!block)
        error(*previous(), "invalid node block");

    return ast::make_expr<ast::struct_aggregate_expr>(name->get_value(), std::move(block));
}

ast::expr_ref<ast::namespace_aggregate_expr> parser::parse_namespace()
{
    consume(token::kind::keyword); // namespace
    auto name = consume(token::kind::identifier);
    if (!name)
        error(*previous(), "expected a identifier after namespace keyword");

    auto next = current();
    if (!next)
        error(*previous(), "expected block found EOF.");

    if (!next->is_block_start())
        error(*previous(), "expected block start, got {}", next->get_kind());

    advance();
    auto block = parse_block();
    if (!block)
        error(*previous(), "invalid node block");

    return ast::make_expr<ast::namespace_aggregate_expr>(name->get_value(), std::move(block));
}

ast::expr_ref<ast::extern_directive_expr> parser::parse_extern_directive(void)
{
    auto paren = consume(token::kind::open_paren);
    if (!paren)
        error(*previous(), "expected '(' after \"@extern\"");

    std::vector<ast::expr_ref<ast::expr>> values{};
    while (true)
    {
        auto next = current();
        if (next && next->get_kind() == token::kind::close_paren)
        {
            advance();
            break;
        }

        auto value = parse_expr();
        if (!value)
            error(*previous(), "invalid expression");

        values.push_back(std::move(value));

        next = current();
        if (!next)
            break;
        if (next->get_kind() == token::kind::comma)
            advance();
        else if (next->get_kind() != token::kind::close_paren)
            error(*next, "expected ',' or ')' after expr");
    }

    if (values.size() < 3)
        error(*previous(),
              "\"@extern\" expects the following values: @extern(name, ret, lib, ...args)");

    auto name = std::move(values.at(0));
    auto ret_type = std::move(values.at(1));
    auto lib = std::move(values.at(2));

    // remove name, ret and lib to keep only func params
    if (values.size() >= 3)
        values.erase(values.begin(), values.begin() + 3);

    return ast::make_expr<ast::extern_directive_expr>(name, ret_type, lib, values);
}

ast::expr_ref<ast::import_directive_expr> parser::parse_import_directive(void)
{
    auto paren = consume(token::kind::open_paren);
    if (!paren)
        error(*current(), "expected '(' after \"@import\"");

    auto value_expr = parse_expr();

    paren = consume(token::kind::close_paren);
    if (!paren)
        error(*current(), "unclosed import directive");
    return ast::make_expr<ast::import_directive_expr>(value_expr);
}

} // namespace kpp
