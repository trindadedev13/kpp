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

#include "interpreter.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <expected>
#include <memory>
#include <print>
#include <string>
#include <vector>

#include "ast.hpp"
#include "error.hpp"
#include "ffi/fn.hpp"
#include "ffi/type.hpp"
#include "function.hpp"
#include "method.hpp"
#include "object/o_block.hpp"
#include "object/o_bool.hpp"
#include "object/o_function.hpp"
#include "object/o_int.hpp"
#include "object/o_method.hpp"
#include "object/o_null.hpp"
#include "object/o_string.hpp"
#include "object/o_struct.hpp"
#include "object/object.hpp"

namespace kpp
{

inline std::string upper_case(std::string s)
{
    std::transform(s.begin(),
                   s.end(),
                   s.begin(),
                   [](unsigned char c) -> char
                   {
                       return std::toupper(c);
                   });
    return s;
}

static std::string get_lib_full(std::string const &name)
{
#define prefix "lib"
#define sufix ".so"

    return prefix + name + sufix;
}

interpreter::interpreter(ast::program program_)
    : m_program{std::move(program_)}
{
    m_scopes.emplace_back();
}

auto interpreter::run(void) -> obj::object_ref<object>
{
    auto &body = m_program.body;
    if (body.empty())
        return obj::make_object<obj::null_object>();

    obj::object_ref<object> last;
    for (auto &expr : body)
        last = expr->visit(*this);

    return last;
}

std::expected<obj::object_ref<object>, std::string> interpreter::lookup(std::string const &name)
{
    for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); it++)
        if (it->contains(name))
            return it->get(name);
    return std::unexpected{std::format("undefined object: {}", name)};
}

std::expected<obj::object_ref<object>, std::string> interpreter::assign(std::string const &name,
                                                                        obj::object_ref<object> obj)
{
    for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); it++)
        if (it->contains(name))
            if (it->set(name, obj))
                return obj;

    return std::unexpected{std::format("undefined object: {}", name)};
}

auto interpreter::visit(ast::literal_expr<std::string> const &expr) -> obj::object_ref<object>
{
    return obj::make_object<obj::string_object>(expr.val);
}

auto interpreter::visit(ast::literal_expr<int> const &expr) -> obj::object_ref<object>
{
    return obj::make_object<obj::int_object>(expr.val);
}

auto interpreter::visit(ast::literal_expr<bool> const &expr) -> obj::object_ref<object>
{
    return obj::make_object<obj::bool_object>(expr.val);
}

auto interpreter::visit(ast::literal_expr<std::nullptr_t> const &expr) -> obj::object_ref<object>
{
    return obj::make_object<obj::null_object>();
}

auto interpreter::visit(ast::block_expr const &expr) -> obj::object_ref<object>
{
    return obj::make_object<obj::block_object>(&expr);
}

auto interpreter::visit(ast::function_expr const &expr) -> obj::object_ref<object>
{
    auto obj = obj::make_object<obj::function_object>(function{&expr});
    current_scope().define(expr.name, obj);
    return obj;
}

inline std::vector<obj::object_ref<object>>
to_objects(ast::expr_visitor &visitor, std::vector<ast::expr_ref<ast::expr>> const &args)
{
    std::vector<obj::object_ref<object>> vs;
    for (auto &expr : args)
    {
        auto v = expr->visit(visitor);
        vs.push_back(v);
    }
    return vs;
}

auto interpreter::run_block(ast::block_expr const &block) -> obj::object_ref<object>
{
    m_scopes.emplace_back();

    obj::object_ref<object> last;
    for (auto &e : block.body)
        /** the last expr is the return value of block */
        last = e->visit(*this);

    m_scopes.pop_back();

    return last;
}

auto interpreter::call_function(function const &fn,
                                obj::object_ref<object> self,
                                std::vector<obj::object_ref<object>> args)
    -> obj::object_ref<object>
{
    /** run native function */
    if (fn.is_native())
    {
        auto nfn = fn.as_native();

        /** ffi is used when using extern declared function */
        if (auto ffi_fn = nfn->get_ffi_fn())
            return (ffi_fn.value())->call(args);

        if (auto cb = nfn->get_callback())
            return (cb.value())(self, args);
    }

    /** run kpp writeen functions */
    if (fn.is_common())
    {
        m_scopes.emplace_back();

        auto *cfn = fn.as_common();

        /** define arguments in scope */
        for (size_t i = 0; i < cfn->params.size(); ++i)
            current_scope().define(cfn->params[i], args[i]);

        return run_block(*cfn->block);
    }

    panic("invalid function");
}

auto interpreter::visit(ast::call_expr const &expr) -> obj::object_ref<object>
{
    auto callee = expr.callee->visit(*this);

    /** call function */
    if (callee->get_type() == obj::get_function_type())
    {
        auto args = to_objects(*this, expr.args);
        return call_function(obj::obj_cast<function const &>(*callee), nullptr, std::move(args));
    }

    /** call method */
    if (callee->get_type() == obj::get_method_type())
    {
        auto const &mtd = obj::obj_cast<method const &>(*callee);
        auto args = to_objects(*this, expr.args);
        return call_function(mtd.fn, mtd.self, std::move(args));
    }

    /** run block */
    if (callee->get_type() == obj::get_block_type())
    {
        auto const *block = obj::obj_cast<ast::block_expr const *>(*callee);
        return run_block(*block);
    }

    std::println("{}", *callee->get_type());

    todo();
}

auto interpreter::visit(ast::var_decl_expr const &expr) -> obj::object_ref<object>
{
    auto val = expr.val->visit(*this);
    current_scope().define(expr.name, val);
    return val;
}

auto interpreter::visit(ast::return_expr const &expr) -> obj::object_ref<object>
{
    todo();
}

auto interpreter::visit(ast::binary_expr const &expr) -> obj::object_ref<object>
{
    auto call_mtd = [this](auto &left, auto &right, auto const &name)
    {
        auto mtd_o = left->get_method(name);
        if (!mtd_o)
            panic("Method '{}(other)' not found for {}", name, left->get_name());
        auto const &mtd = obj::obj_cast<method const &>(*mtd_o.value());
        auto args = std::vector{right};
        return call_function(mtd.fn, left, args);
    };

    auto left = expr.left->visit(*this);
    auto right = expr.right->visit(*this);
    switch (expr.kind)
    {
    case ast::binary_expr::op_kind::add:
        return call_mtd(left, right, "plus");
    case ast::binary_expr::op_kind::sub:
        return call_mtd(left, right, "minus");
    case ast::binary_expr::op_kind::mul:
        return call_mtd(left, right, "times");
    case ast::binary_expr::op_kind::div:
        return call_mtd(left, right, "div");
    default:
        todo();
    }
}

auto interpreter::visit(ast::assignment_expr const &expr) -> obj::object_ref<object>
{
    auto value = expr.val->visit(*this);

    if (ctx == context::directive)
        return value;

    if (auto *id = dynamic_cast<ast::identifier_expr *>(expr.target.get()))
    {
        if (auto res = assign(id->name, value))
            return value;
        else
            panic("{}", res.error());
    }

    if (auto *member = dynamic_cast<ast::member_access_expr *>(expr.target.get()))
    {
        auto obj = member->obj->visit(*this);
        if (auto res = obj->set_property(member->name, value))
            return value;
        else
            panic("{}", res.error());
    }
    todo();
}

auto interpreter::visit(ast::identifier_expr const &expr) -> obj::object_ref<object>
{
    /** lookuo in the current scope */
    if (auto s = lookup(expr.name))
        return s.value();

    /** look in type table */
    if (auto t = obj::get_type(expr.name))
        return t.value();

    panic("undefined object or type {}", expr.name);
}

auto interpreter::visit(ast::member_access_expr const &expr) -> obj::object_ref<object>
{
    auto obj = expr.obj->visit(*this);

    if (auto mtd = obj->get_method(expr.name); mtd)
    {
        auto const &mt = obj::obj_cast<method const &>(*mtd.value());
        return obj::make_object<obj::method_object>(method{obj, mt.fn});
    }

    if (obj->get_type() != obj::get_type_type())
    {
        if (auto mtd = obj->get_type()->get_method(expr.name); mtd)
        {
            auto const &mt = obj::obj_cast<method const &>(*mtd.value());
            return obj::make_object<obj::method_object>(method{obj, mt.fn});
        }
    }

    if (auto prop = obj->get_property(expr.name); prop)
        return prop.value();

    panic("neither property nor method found for name {} in {}", expr.name, obj->get_name());
}

auto interpreter::visit(ast::namespace_access_expr const &expr) -> obj::object_ref<object>
{
    auto obj = expr.obj->visit(*this);

    auto type = std::dynamic_pointer_cast<obj::type_object>(obj);
    if (!type)
        panic("'::' can only be used on types");

    /** static method */
    if (auto mtd = type->get_static_method(expr.name); mtd)
        return mtd.value();

    /** static property */
    if (auto prop = type->get_static_property(expr.name); prop)
        return prop.value();

    panic("neither static property nor static method found for name {}", expr.name);
}

auto interpreter::visit(ast::aggregate_expr const &expr) -> obj::object_ref<object>
{
    todo();
}

auto interpreter::visit(ast::struct_aggregate_expr const &expr) -> obj::object_ref<object>
{
    auto st_obj = obj::make_object<obj::struct_object>(&expr);
    auto struct_type = obj::make_object<obj::type_object>(expr.name, obj::get_type_type());

    for (auto &e : expr.block->body)
    {
        if (auto *fn = dynamic_cast<ast::function_expr *>(e.get()))
        {
            auto obj_fn = obj::make_object<obj::function_object>(function{fn});
            if (!fn->params.empty() && fn->params.at(0) == "self")
                struct_type->define_method(fn->name, obj_fn);
            else
                struct_type->define_static_method(fn->name, obj_fn);
        }
        else if (auto *var = dynamic_cast<ast::var_decl_expr *>(e.get()))
        {
            if (var->name != upper_case(var->name))
                st_obj->define_property(var->name, var->val->visit(*this));
            else
                struct_type->define_static_property(var->name, var->val->visit(*this));
        }
    }

    st_obj->set_type(struct_type);
    obj::deftypes.user_types[expr.name] = struct_type;

    return st_obj;
}

auto interpreter::visit(ast::namespace_aggregate_expr const &expr) -> obj::object_ref<object>
{
    todo();
}

auto interpreter::visit(ast::extern_directive_expr const &expr) -> obj::object_ref<object>
{
    ctx = context::directive;
    auto name = expr.name->visit(*this)->to_str();
    auto ret = expr.ret_type->visit(*this)->to_str();
    auto lib = expr.lib->visit(*this)->to_str();

    auto so = soman.load(get_lib_full(lib));
    auto sym = so->sym(name);
    if (!sym)
        panic("no sym found for {} from {} due to {}", name, lib, so->get_error());

    auto ct_ret = ffi::to_c_type(ret);
    std::vector<ffi::c_type> ct_pts{};
    for (auto &p : expr.params_types)
    {
        auto s = p->visit(*this)->to_str();
        auto ct = ffi::to_c_type(s);
        if (ct)
            ct_pts.push_back(ct.value());
        else
            panic("expected a valid CType as param type, not {}", s);
    }

    if (!ct_ret)
        panic("expected a valid CType as return type, not {}", ret);

    auto f = std::make_shared<ffi::fn>(ct_pts, ct_ret.value(), sym);
    auto n = std::make_shared<native_function>(name, f, ct_ret.value());
    auto o = obj::make_object<obj::function_object>(n);
    current_scope().define(name, o);
    ctx = context::default_;
    return o;
}

auto interpreter::visit(ast::import_directive_expr const &expr) -> obj::object_ref<object>
{
    todo();
}

} // namespace kpp
