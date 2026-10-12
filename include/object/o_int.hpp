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

#include <print>
#include <string>

#include "object/object.hpp"

namespace kpp::obj
{

template <>
inline int obj_cast<int>(object const &value);

struct int_object : public object
{
    explicit int_object(int value)
        : object{get_int_type()},
          m_value(value)
    {
    }

    std::string get_name(void) const override
    {
        return "int";
    }

    std::string to_str() const override
    {
        return std::to_string(get_value());
    }

    std::string repr() const override
    {
        return std::format("<int {}>", get_value());
    }

    int get_value() const
    {
        return m_value;
    }

    static void register_methods(void)
    {
        auto type = get_int_type();
        type->define_method(make_method(type,
                                        "plus",
                                        [](auto self, auto const &args)
                                        {
                                            return make_object<int_object>(
                                                obj_cast<int>(*self) + obj_cast<int>(*args.at(0)));
                                        }));

        type->define_method(make_method(type,
                                        "minus",
                                        [](auto self, auto const &args)
                                        {
                                            return make_object<int_object>(
                                                obj_cast<int>(*self) - obj_cast<int>(*args.at(0)));
                                        }));

        type->define_method(make_method(type,
                                        "times",
                                        [](auto self, auto const &args)
                                        {
                                            return make_object<int_object>(
                                                obj_cast<int>(*self) * obj_cast<int>(*args.at(0)));
                                        }));

        type->define_method(make_method(type,
                                        "div",
                                        [](auto self, auto const &args)
                                        {
                                            return make_object<int_object>(
                                                obj_cast<int>(*self) / obj_cast<int>(*args.at(0)));
                                        }));
    }

private:
    int m_value;
};

template <>
inline int obj_cast<int>(object const &value)
{
    return static_cast<int_object const &>(value).get_value();
}

} // namespace kpp::obj
