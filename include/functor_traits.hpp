#pragma once

#include <functional>

template <typename Functor>
struct functor_traits : functor_traits<decltype(&Functor::operator())> {};

template <typename ClassType, typename ReturnType, typename... ArgTypes>
struct functor_traits<ReturnType(ClassType::*)(ArgTypes...) const>
{
   typedef std::function<ReturnType(ArgTypes...)> function_type;
};
