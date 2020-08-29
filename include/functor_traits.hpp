#pragma once

#include <functional>

template <typename Functor>
struct functor_traits : functor_traits<decltype(&Functor::operator())> {};

template <typename ClassType, typename ReturnType, typename... ArgTypes>
struct functor_traits<ReturnType(ClassType::*)(ArgTypes...) const>
{
   using function_type = std::function<ReturnType(ArgTypes...)>;

   struct nargs : std::integral_constant<size_t, sizeof...(ArgTypes)> {};

   template <size_t I>
   struct arg : std::tuple_element<I, std::tuple<ArgTypes...>> {};

   template<size_t I>
   using arg_t = typename arg<I>::type;
};
