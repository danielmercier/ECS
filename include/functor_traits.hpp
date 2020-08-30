#pragma once
#include <functional>

template <typename Functor>
struct functor_traits : functor_traits<decltype(&Functor::operator())> {};

template <typename ClassType, typename ReturnType, typename... ArgTypes>
struct functor_traits<ReturnType(ClassType::*)(ArgTypes...) const>
{
   using function_type = std::function<ReturnType(ArgTypes...)>;

   using args_t = std::tuple<ArgTypes...>;

   using args_decay_t = std::tuple<std::decay_t<ArgTypes>...>;

   using args_pointer_t = std::tuple<std::decay_t<ArgTypes>*...>;
};
