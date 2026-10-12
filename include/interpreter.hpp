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
#include <expected>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ast.hpp"
#include "ffi/so.hpp"
#include "function.hpp"
#include "object/object.hpp"

namespace kpp
{

struct scope
{
    void define(std::string name, obj::object_ref<object> obj)
    {
        m_values.emplace(std::move(name), std::move(obj));
    }

    std::expected<void, std::string> set(std::string const &name, obj::object_ref<object> obj)
    {
        auto it = m_values.find(name);
        if (it == m_values.end())
            return std::unexpected{std::format("no object {} in current scopes", name)};

        it->second = std::move(obj);
        return {};
    }

    bool contains(std::string const &name) const
    {
        return m_values.contains(name);
    }

    obj::object_ref<object> &get(std::string const &name)
    {
        return m_values.at(name);
    }

private:
    std::unordered_map<std::string, obj::object_ref<object>> m_values;
};

struct so_manager
{
    std::shared_ptr<ffi::so> load(std::string const &path)
    {
        auto it = libraries.find(path);

        if (it != libraries.end())
            return it->second;

        auto lib = std::make_shared<ffi::so>(path);

        libraries.emplace(path, lib);
        return lib;
    }

private:
    std::unordered_map<std::string, std::shared_ptr<ffi::so>> libraries;
};

struct interpreter : ast::expr_visitor
{
    interpreter(ast::program);

    obj::object_ref<object> run(void);

private:
    enum class context
    {
        default_ = 0,
        directive
    };

    ast::program m_program;
    std::vector<scope> m_scopes;
    context ctx;
    so_manager soman;

    scope &current_scope(void)
    {
        return m_scopes.back();
    }

    auto lookup(std::string const &) -> std::expected<obj::object_ref<object>, std::string>;
    auto assign(std::string const &, obj::object_ref<object>)
        -> std::expected<obj::object_ref<object>, std::string>;

    auto call_function(function const &fn,
                       obj::object_ref<object> self,
                       std::vector<obj::object_ref<object>> args) -> obj::object_ref<object>;
    auto run_block(ast::block_expr const &) -> obj::object_ref<object>;

    auto visit(ast::literal_expr<std::string> const &) -> obj::object_ref<object> override;
    auto visit(ast::literal_expr<int> const &) -> obj::object_ref<object> override;
    auto visit(ast::literal_expr<bool> const &) -> obj::object_ref<object> override;

    auto visit(ast::literal_expr<std::nullptr_t> const &) -> obj::object_ref<object> override;
    auto visit(ast::block_expr const &) -> obj::object_ref<object> override;
    auto visit(ast::function_expr const &) -> obj::object_ref<object> override;
    auto visit(ast::call_expr const &) -> obj::object_ref<object> override;
    auto visit(ast::var_decl_expr const &) -> obj::object_ref<object> override;
    auto visit(ast::return_expr const &) -> obj::object_ref<object> override;
    auto visit(ast::binary_expr const &) -> obj::object_ref<object> override;
    auto visit(ast::assignment_expr const &) -> obj::object_ref<object> override;
    auto visit(ast::identifier_expr const &) -> obj::object_ref<object> override;
    auto visit(ast::member_access_expr const &) -> obj::object_ref<object> override;
    auto visit(ast::namespace_access_expr const &) -> obj::object_ref<object> override;
    auto visit(ast::aggregate_expr const &) -> obj::object_ref<object> override;
    auto visit(ast::struct_aggregate_expr const &) -> obj::object_ref<object> override;
    auto visit(ast::namespace_aggregate_expr const &) -> obj::object_ref<object> override;

    auto visit(ast::extern_directive_expr const &) -> obj::object_ref<object> override;
    auto visit(ast::import_directive_expr const &) -> obj::object_ref<object> override;
};

} // namespace kpp
