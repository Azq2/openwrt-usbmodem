#pragma once

#include <type_traits>

#define ENUM_UNDERLYING_CAST(T, v) static_cast<std::underlying_type<T>::type>(v)
#define DEFINE_ENUM_BIT_OPERATORS(T) \
	constexpr T operator|(T a, T b) { return static_cast<T>(ENUM_UNDERLYING_CAST(T, a) | ENUM_UNDERLYING_CAST(T, b)); } \
	constexpr T operator|=(T &a, T b) { return a | b; } \
	constexpr T operator&(T a, T b) { return static_cast<T>(ENUM_UNDERLYING_CAST(T, a) & ENUM_UNDERLYING_CAST(T, b)); } \
	constexpr T operator&=(T &a, T b) { return a & b; } \
	constexpr T operator~(T a) { return static_cast<T>(~ENUM_UNDERLYING_CAST(T, a)); }
