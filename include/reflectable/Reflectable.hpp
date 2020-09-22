#pragma once

#include "TypeTraits.hpp"

#include <cstdint>
#include <tuple>

namespace ReflectLib {
/**
 * Empty base class for all Reflectable types that don't derive from another
 * Reflectable type
 */
struct ReflectableBase {
  template <typename TOperation>
  static constexpr int _reflectable_for_each_impl(TOperation&&, size_t) {
    return 0;
  }

  struct Attr_RequiredMember {};

  struct Attr_None {};

  /**
   * @brief Attribute that causes command line parameter to be ignored.
   *
   * Allows using configs with unsupported types
   */
  template <typename... Ts>
  struct Attr_Ignore : type_pack<Ts...> {};
};

template <typename T>
using is_reflectable = std::is_base_of<ReflectableBase, T>;

#define SIMPLE_ATTRIBUTE(name, type_name, ...)                         \
  struct name {                                                        \
    constexpr name(type_name value = {__VA_ARGS__}) : value_{value} {} \
    const std::remove_cv<type_name>::type value_;                      \
  };

#define TRY_GET_ATTR(attr_collection, attr_type, attr_val)                  \
  if constexpr (ReflectLib::tuple_has_type<                                 \
                    attr_type,                                              \
                    typename std::remove_cv<typename std::remove_reference< \
                        decltype(attr_collection)>::type>::type>::value)    \
    if constexpr ([[maybe_unused]] const auto& attr_val =                   \
                      std::get<attr_type>(attr_collection);                 \
                  true)

#define IF_NOT_ATTR(attr_collection, attr_type)                             \
  if constexpr (!ReflectLib::tuple_has_type<                                \
                    attr_type,                                              \
                    typename std::remove_cv<typename std::remove_reference< \
                        decltype(attr_collection)>::type>::type>::value)

/**
 * Begins enumerable member block
 *
 * Defines hidden function static _reflectable_for_each_impl(op, ordinal)
 *
 * op       - operation to apply to all member definition.
 * ordinal  - starting value of ordinal for enumerating members.
 *
 * op is expected to have following signature:
 * Result op(size_t ordinal, const char *name, member_info_t m, std::tuple<...>
 * attrs)
 *
 * @code
 *
 * SIMPLE_ATTRIBUTE(Attr_Decimals, int, 2);
 * SIMPLE_ATTRIBUTE(Attr_Scientific, bool, true);
 *
 * struct Foo : Reflectable<Foo>
 * {
 *      BEGIN_REFLECTABLE_MEMBERS()
 *
 *      REFLECTABLE_MEMBER(int, bar)
 *      REFLECTABLE_MEMBER_ATTRS(
 *          double, bar,
 *          std::make_tuple(Attr_Decimals(4), Attr_Scientific())
 *          22)
 *
 *      END_REFLECTABLE_MEMBERS()
 * };
 * @endcode
 */
#define BEGIN_REFLECTABLE_MEMBERS()                                        \
 public:                                                                   \
  template <typename TOperation>                                           \
  constexpr static int _reflectable_for_each_impl(TOperation&& op,         \
                                                  size_t ordinal) {        \
    const int result = static_cast<int>(_reflectable_for_each_member_base( \
        std::forward<TOperation>(op), ordinal));                           \
    if (result < 0) {                                                      \
      return result;                                                       \
    }                                                                      \
    ordinal = static_cast<size_t>(result);

/**
 * Implementation of @see REFLECTABLE_MEMBER or @see
 * REFLECTABLE_MEMBER_WITH_ATTRS. Should not be used directly
 */
#define REFLECTABLE_MEMBER_IMPL(type_name, name, attrs, ...)                   \
  if (!op(ordinal, #name,                                                      \
          ::ReflectLib::member_t<                                              \
              self_t, ::ReflectLib::argument_type<void(type_name)>::type,      \
              &self_t::name>{},                                                \
          attrs)) {                                                            \
    return false;                                                              \
  } else {                                                                     \
    return _reflectable_for_each_continue_##name(std::forward<TOperation>(op), \
                                                 ordinal + 1);                 \
  }                                                                            \
  }                                                                            \
  typename ReflectLib::argument_type<void(type_name)>::type name{__VA_ARGS__}; \
  template <typename TOperation>                                               \
  constexpr static int _reflectable_for_each_continue_##name(                  \
      [[maybe_unused]] TOperation&& op, size_t ordinal) {
/**
 * Defines enumerable member without attributes and with optional initializer.
 *
 * @see BEGIN_REFLECTABLE_MEMBERS for usage details
 */
#define REFLECTABLE_MEMBER(type, name, ...) \
  REFLECTABLE_MEMBER_IMPL(                  \
      type, name, std::make_tuple(ReflectableBase::Attr_None{}), __VA_ARGS__)

/**
 * Defines enumerable member with attributes and with optional initializer.
 *
 * @see BEGIN_REFLECTABLE_MEMBERS for usage details
 */
#define REFLECTABLE_MEMBER_ATTRS(type, name, attrs, ...) \
  REFLECTABLE_MEMBER_IMPL(type, name, attrs, __VA_ARGS__)

#define END_REFLECTABLE_MEMBERS() \
  return ordinal;                 \
  }

template <typename TStruct, typename TMember, TMember TStruct::*member_ptr>
struct member_t {
  using type = TMember;

  static TMember& get(TStruct& s) { return s.*member_ptr; }

  static const TMember& get(const TStruct& s) { return s.*member_ptr; }

  TMember& operator()(TStruct& s) { return get(s); }

  const TMember& operator()(const TStruct& s) { return get(s); }
};

template <typename T>
static constexpr size_t reflectable_count() {
  size_t count = 0;
  (void)T::_reflectable_for_each_impl(
      [&](size_t, const char*, auto, const auto&) {
        ++count;
        return true;
      },
      0);

  return count;
}

template <typename TSelf, typename TBase = ReflectableBase>
struct Reflectable : TBase {
 public:
  using self_t = TSelf;
  using base_t = TBase;

  template <typename TOperation>
  static constexpr int _reflectable_for_each_member_base(TOperation&& op,
                                                         size_t ordinal) {
    return TBase::_reflectable_for_each_impl(std::forward<TOperation>(op),
                                             ordinal);
  }

  template <typename TOperation>
  static constexpr int for_each_member(TOperation&& member_op) {
    return TSelf::_reflectable_for_each_impl(
        std::forward<TOperation>(member_op), 0);
  }

  /**
   * @brief Invokes op() for each enumerable member. Breaks early if op()
   * returns failure
   *
   * Op must be either templated functor for lambda with folling signature:
   *
   * Result op(size_t ordinal, const char *name, TMember &member,
   * std::tuple<TAttributes...> &attrs);
   *
   * @tparam  TOperation  Type of the operation to apply to all members
   *
   * @param   value_op    Operation to apply to all members
   */
  template <typename TOperation>
  constexpr int for_each_member_value(TOperation&& value_op) {
    return TSelf::_reflectable_for_each_impl(
        [=](size_t ordinal, const char* name, auto m, const auto& attrs) {
          return value_op(ordinal, name, m(static_cast<TSelf&>(*this)), attrs);
        },
        0);
  }

  /**
   * @brief Returns reference to *this cast to the child class type
   *
   * @return Reference to self.
   */
  TSelf& _self() { return static_cast<TSelf&>(*this); }

  TBase& _base() { return static_cast<TBase&>(*this); }
};

/**
 * @brief Empty Reflectable
 */
struct Empty : Reflectable<Empty> {
  BEGIN_REFLECTABLE_MEMBERS()
  END_REFLECTABLE_MEMBERS()
};

}  // namespace ReflectLib
