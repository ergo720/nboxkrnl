/*
 * ergo720                Copyright (c) 2022
 * cxbxr devs
 */

#pragma once

#include <type_traits>


// NOTE: with C++23, this can be replaced with std::to_underlying
template <typename E>
consteval typename std::underlying_type<E>::type to_underlying(E e) noexcept
{
	return static_cast<std::underlying_type_t<E>>(e);
}
