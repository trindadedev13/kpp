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

#include "object/object.hpp"
#include "function.hpp"
#include "method.hpp"
#include "object/o_int.hpp"
#include "object/o_method.hpp"
#include "object/o_null.hpp"
#include "object/o_string.hpp"

#include <memory>
#include <string>
#include <vector>

namespace kpp
{

object::object(obj::object_ref<obj::type_object> type_)
    : m_type{type_}
{
}

void object::define_property(std::string const &name, obj::object_ref<object> value)
{
    m_type->define_property(name, value);
}

auto object::set_property(std::string const &name, obj::object_ref<object> &value)
    -> std::expected<void, std::string>
{
    return m_type->set_property(name, value);
}

auto object::get_method(std::string const &name)
    -> std::expected<obj::object_ref<object>, std::string>
{
    if (auto it = m_type->find_method(name); it != m_type->methods_end())
        return it->second;

    return std::unexpected{std::format("undefined method: {}", name)};
}

auto object::get_property(std::string const &name)
    -> std::expected<obj::object_ref<object>, std::string>
{
    if (auto it = m_type->find_property(name); it != m_type->properties_end())
        return it->second;

    return std::unexpected{std::format("undefined variable: {}", name)};
}

/********** BEGIN _TYPES ***********/
obj::_types obj::deftypes = _types::create();

obj::_types obj::_types::create()
{
    /** helper to make a type obj */
    static auto _cr =
        [](auto const &name, auto meta, auto base_ = nullptr) -> object_ref<type_object>
    {
        auto base = base_;
        return make_object<type_object>(name, meta, base);
    };

    _types ts;

    /** create type */
    auto &meta_base = ts.builtin_types["type"] = _cr("type", nullptr, nullptr);
    meta_base->set_type(meta_base);

    ts.builtin_types["null"] = _cr("null", meta_base, meta_base);
    ts.builtin_types["int"] = _cr("int", meta_base, meta_base);
    ts.builtin_types["bool"] = _cr("bool", meta_base, meta_base);
    ts.builtin_types["string"] = _cr("string", meta_base, meta_base);
    ts.builtin_types["block"] = _cr("block", meta_base, meta_base);
    ts.builtin_types["function"] = _cr("function", meta_base, meta_base);
    ts.builtin_types["method"] = _cr("method", meta_base, meta_base);
    ts.builtin_types["struct"] = _cr("struct", meta_base, meta_base);

    type_object::register_methods(get_type_type());
    int_object::register_methods();
    string_object::register_methods();

    return ts;
}

obj::object_ref<object> obj::make_method(
    obj::object_ref<object> self,
    std::string const &name,
    std::function<obj::object_ref<object>(object_ref<object> self,
                                          std::vector<obj::object_ref<object>> const &args)> fn)
{
    auto n = std::make_shared<native_function>(name, fn);
    auto f = function{n};
    auto m = method{self, std::move(f)};
    return obj::make_object<obj::method_object>(m);
};

/********** BEGIN TYPE OBJECT ******/

auto obj::type_object::get_static_method(std::string const &name)
    -> std::expected<obj::object_ref<object>, std::string>
{
    if (auto it = static_methods.find(name); it != static_methods.end())
        return it->second;

    if (base)
        return base->get_static_method(name);

    return std::unexpected{std::format("static method {} not defined", name)};
}

auto obj::type_object::get_static_property(std::string const &name)
    -> std::expected<obj::object_ref<object>, std::string>
{
    if (auto it = static_properties.find(name); it != static_properties.end())
        return it->second;

    if (base)
        return base->get_static_property(name);

    return std::unexpected{std::format("static property {} not defined", name)};
}

void obj::type_object::define_static_method(std::string const &name, obj::object_ref<object> m)
{
    m_type->static_methods[name] = m;
}

void obj::type_object::define_static_property(std::string const &name, obj::object_ref<object> p)
{
    m_type->static_properties[name] = p;
}

auto obj::type_object::set_static_property(std::string const &name, obj::object_ref<object> value)
    -> std::expected<void, std::string>
{
    auto it = m_type->static_properties.find(name);

    if (it == m_type->static_properties.end())
        return std::unexpected{std::format("property {} not defined", name)};

    it->second = std::move(value);
    return {};
}

void obj::type_object::register_methods(obj::object_ref<type_object> to)
{
    to->define_static_method(
        make_method("new",
                    [](object_ref<object> self,
                       std::vector<object_ref<object>> const &args) -> object_ref<object>
                    {
                        return make_object<null_object>();
                    }));

    /** get_type instance method */
    to->define_method(
        make_method(to,
                    "get_type",
                    [](object_ref<object> self,
                       std::vector<object_ref<object>> const &args) -> object_ref<object>
                    {
                        return self->get_type();
                    }));

    /** get_type type method */
    to->define_static_method(
        make_method(to,
                    "get_type",
                    [](object_ref<object> self,
                       std::vector<object_ref<object>> const &args) -> object_ref<object>
                    {
                        return self->get_type();
                    }));

    /** to_str instance method */
    to->define_method(
        make_method(to,
                    "to_str",
                    [](object_ref<object> self,
                       std::vector<object_ref<object>> const &args) -> object_ref<object>
                    {
                        return make_object<string_object>(self->to_str());
                    }));

    to->define_method(
        make_method(to,
                    "get_name",
                    [](object_ref<object> self,
                       std::vector<object_ref<object>> const &args) -> object_ref<object>
                    {
                        return make_object<string_object>(self->get_name());
                    }));

    to->define_method(
        make_method(to,
                    "repr",
                    [](object_ref<object> self,
                       std::vector<object_ref<object>> const &args) -> object_ref<object>
                    {
                        return make_object<string_object>(self->repr());
                    }));
}

/********** END TYPE OBJECT ********/

} // namespace kpp
