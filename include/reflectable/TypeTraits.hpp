#pragma once

#include <tuple>
#include <type_traits>
#include <variant>

namespace ReflectLib {
template <typename T>
struct argument_type;

/**
 * @brief returns type of the only function argument
 */
template <typename T, typename U>
struct argument_type<T(U)> {
  typedef U type;
};

/**
 * @brief Evaluates to std::false_type or std::true_type depending on tuple
 * containing a given type
 */
template <typename T, typename Tuple>
struct tuple_has_type;

template <typename T>
struct tuple_has_type<T, std::tuple<>> : std::false_type {};

template <typename T, typename U, typename... Ts>
struct tuple_has_type<T, std::tuple<U, Ts...>>
    : tuple_has_type<T, std::tuple<Ts...>> {};

template <typename T, typename... Ts>
struct tuple_has_type<T, std::tuple<T, Ts...>> : std::true_type {};

/**
 * @brief Evaluates to std::false_type or std::true_type depending T being a
 * specialization of given Template
 */
template <template <typename...> class Template, typename T>
struct is_specialization_of : std::false_type {};

template <template <typename...> class Template, typename... Args>
struct is_specialization_of<Template, Template<Args...>> : std::true_type {};

/**
 * @brief Evaluates to std::false_type or std::true_type depending T being a
 * specialization of std::array
 */
template <typename T>
struct is_std_array : std::false_type {};

template <typename value_type, size_t size>
struct is_std_array<std::array<value_type, size>> : std::true_type {};

/**
 * @brief Iterates over all elements of tuple
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-parameter"
template <size_t i = 0, class F, class... Ts>
void for_each_in_const_tuple(const std::tuple<Ts...>& tuple, F func) {
  if constexpr (i < sizeof...(Ts)) {
    (void)func(std::get<i>(tuple));
    for_each_in_const_tuple<i + 1>(tuple, func);
  }
}

template <size_t i = 0, class F, class... Ts>
void for_each_in_tuple(std::tuple<Ts...>& tuple, F func) {
  if constexpr (i < sizeof...(Ts)) {
    (void)func(std::get<i>(tuple));
    for_each_in_tuple<i + 1>(tuple, func);
  }
}
#pragma GCC diagnostic pop

/**
 * @brief Returns type name
 */
template <typename T>
constexpr const char* get_type_name() {
  const char* name = __PRETTY_FUNCTION__;

  while (*name++ != '[')
    ;
  for (auto skip_space = 3; skip_space; --skip_space) {
    while (*name++ != ' ')
      ;
  }

  return name;
}

template <typename T>
struct type_t {
  using type = T;
};

/**
 * @brief Pack for passing types around
 */
template <typename... T>
struct type_pack {
  /**
   * @brief Number of types in the pack
   */
  static constexpr size_t size = sizeof...(T);

  template <size_t index, typename... Ts>
  struct get_impl {};

  template <size_t index, typename TOnly>
  struct get_impl<index, TOnly> {
    using type = TOnly;
  };

  template <size_t index, typename TFirst, typename... TRest>
  struct get_impl<index, TFirst, TRest...> {
    using type = typename std::conditional<
        index == 0,
        TFirst,
        typename get_impl<index - 1, TRest...>::type>::type;
  };

  /**
   * @brief Returns type from the pack at a given index
   */
  template <size_t index>
  struct get {
    static_assert(index < size, "Invalid type index");
    using type = typename get_impl<index, T...>::type;
  };

  template <typename S, size_t i = 0>
  static constexpr bool contains() {
    if constexpr (i < size) {
      return std::is_same<typename get<i>::type, S>::value ||
             contains<S, i + 1>();
    }

    return false;
  }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-parameter"
  template <size_t index = 0, typename F, typename... TArgs>
  static constexpr void for_each(F f, TArgs&&... args) {
    if constexpr (index < size) {
      f(type_t<typename get<index>::type>{}, std::forward(args)...);
      for_each<index + 1, F, TArgs...>(f, std::forward(args)...);
    }
  }
};
#pragma GCC diagnostic pop

/**
 * @brief Helper for std::visit
 */
template <class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

/**
 * @brief is_container conditional
 */
template <typename T, typename _ = void>
struct is_container : std::false_type {};

template <typename... Ts>
struct is_container_helper {};

template <typename T>
struct is_container<
    T,
    std::conditional_t<false,
                       is_container_helper<typename T::value_type,
                                           typename T::size_type,
                                           typename T::allocator_type,
                                           typename T::iterator,
                                           typename T::const_iterator,
                                           decltype(std::declval<T>().size()),
                                           decltype(std::declval<T>().begin()),
                                           decltype(std::declval<T>().end()),
                                           decltype(std::declval<T>().cbegin()),
                                           decltype(std::declval<T>().cend())>,
                       void>> : public std::true_type {};

template <typename T, typename... N>
auto make_array(N&&... args) -> std::array<T, sizeof...(args)> {
  return {{std::forward<N>(args)...}};
}
}  // namespace ReflectLib
