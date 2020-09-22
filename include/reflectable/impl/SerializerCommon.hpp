#pragma once

#include "reflectable/Reflectable.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <list>
#include <optional>
#include <set>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ReflectLib::Impl {
/**
 * @brief Typedef for option value handlers used in dispatch maps
 */
template <typename T, typename... Targs>
using member_handler_t = bool (*)(T&, Targs...);

/**
 * @brief Dispatch map entry type.
 */
template <typename handler_t>
struct member_entry_t {
  /**
   * @brief Enumerable member name.
   */
  const char* name;
  handler_t handler;
  size_t ordinal;
  bool is_required{false};
};

/**
 * @brief Tracks unique required options seen
 *
 * @tparam T        Configuration type (inferred from the argument)
 * @tparam TBase    Base configuration type (inferred from the argument)
 */
template <typename T>
struct required_members_t {
  required_members_t() : seen_options(reflectable_count<T>(), false) {}

  /**
   * @brief Marks required option identified by ordinal as seen
   *
   * @param ordinal   Option ordinal
   */
  template <typename handler_t, typename... TArgs>
  bool handle(const member_entry_t<handler_t>& entry,
              T& config,
              TArgs&&... args) {
    if (!entry.handler(config, std::forward<TArgs>(args)...)) {
      return false;
    }

    if (entry.is_required && !seen_options[entry.ordinal]) {
      seen_options[entry.ordinal] = true;
      ++unique_required_members_seen;
    }

    return true;
  }

  /**
   * @brief Checks if all required options were seen
   * @return True if all required options were seen
   */
  bool seen_all() const {
    return unique_required_members_seen == required_option_count();
  }

  bool check() const { return seen_all(); }

 private:
  std::vector<bool> seen_options;

  size_t unique_required_members_seen{0};

  static constexpr size_t required_option_count() {
    size_t result = 0;

    T::for_each_member(
        [&](size_t, const char* name, auto m, const auto& attrs) {
          TRY_GET_ATTR(attrs, ReflectableBase::Attr_RequiredMember, a) {
            ++result;
          }

          return true;
        });

    return result;
  }
};

struct Attr_IgnoreNothing {};

/**
 * @brief Tracks range of matching long option names in a sorted array.
 *
 * Generates compile-time sorted array of [string, handler] pairs corresponding
 * to long option names.
 *
 * As names are matched, dashes are replaced with underscores.
 *
 * When option name is parsed:
 * 1) @see reset() method is called.
 * 2) Option name is passed character by character to @see update_match() which
 * updates @see match range alway narrowing it. @see has_match is set only if
 * single match remained when '\0' character was passed. 3) Client can access
 * the handler via @see match() if @has_match is true.
 *
 * @tparam  T       Type of Reflectable with named enumerable members.
 * @tparam  TBase   Base type of the Reflectable.
 */
template <typename T,
          typename handler_t,
          template <typename, typename>
          typename member_handler,
          typename Attr_Ignore = Attr_IgnoreNothing>
struct member_dispatch_t {
  using array_t = std::array<member_entry_t<handler_t>, reflectable_count<T>()>;

  member_dispatch_t() = default;

  /**
   * @brief Range of potentially matching strings.
   */
  std::pair<typename array_t::const_iterator, typename array_t::const_iterator>
      match{};

  /**
   * @brief Position of currently match character.
   */
  size_t name_pos{0};

  /**
   * @brief true if long option name was successfully matched
   */
  bool has_match{false};

  /**
   * @brief Returns matching option value handler.
   */
  const member_entry_t<handler_t>& handler() const { return *match.first; }

  /**
   * @brief Resets matching state.
   */
  void reset() {
    match = std::make_pair(dispatch_map.begin(), dispatch_map.end());
    name_pos = 0;
    has_match = false;
  }

  /**
   * @brief Comparison functor that compares single character with character in
   * fixed position in a string
   */
  struct comparator {
    constexpr comparator(size_t pos) : pos_(pos) {}

    /**
     * @brief Position of the character being compared
     */
    const size_t pos_;

    bool operator()(const typename array_t::value_type& lhs, char rhs) const {
      return lhs.name[pos_] < rhs;
    }

    bool operator()(char lhs, const typename array_t::value_type& rhs) const {
      return lhs < rhs.name[pos_];
    }
  };

  /**
   * @brief Updates @see match with narrowing range of potentially matching
   * option names
   *
   * @param c     Next character to be matched
   */
  void update_match(char c) {
    /* Repalce dashes with underscores */
    c = (c == '-') ? '_' : c;
    match =
        std::equal_range(match.first, match.second, c, comparator{name_pos});
    ++name_pos;

    has_match = c == 0 && (match.second - match.first) == 1;
  }

  /**
   * @brief Matches collection of characters (string or string_view) against map
   */
  template <typename TString>
  bool find_string(const TString& s) {
    reset();
    for (char c : s) {
      update_match(c);
    }

    update_match(0);

    return has_match;
  }

 private:
  /**
   * @brief Returns number of long names that are lexicographically less than
   * the argument.
   *
   * Used for sorting long names in the dispatch table
   *
   * @tparam  T           Reflectable<T> to get names from
   *
   * @param   opt_name    Name to compare with others
   *
   * @returns             Number of long names that are strictly less than the
   * argument
   */
  static constexpr size_t get_long_name_index(const char* opt_name) {
    size_t index = 0;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-parameter"
    T::for_each_member([&](size_t, const char* name, auto, const auto& attrs) {
#pragma GCC diagnostic pop
      IF_NOT_ATTR(attrs, Attr_Ignore) {
        bool is_less = false;
        const unsigned char* left =
            reinterpret_cast<const unsigned char*>(name);
        const unsigned char* right =
            reinterpret_cast<const unsigned char*>(opt_name);

        while (true) {
          if (*left < *right) {
            is_less = true;
            break;
          } else if (*left > *right) {
            break;
          } else if (*left == 0) {
            break;
          }

          ++left;
          ++right;
        }

        if (is_less) {
          ++index;
        }
      }

      return true;
    });

    return index;
  }

  /**
   * @brief Constructs @see dispatch_map
   *
   * @return  Dispatch map
   */
  static constexpr array_t get_member_dispatch() {
    array_t result{};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-parameter"
    T::for_each_member(
        [&](size_t ordinal, const char* name, auto m, const auto& attrs) {
#pragma GCC diagnostic pop
          IF_NOT_ATTR(attrs, Attr_Ignore) {
            const size_t index = get_long_name_index(name);
            result[index].name = name;
            result[index].handler = member_handler<T, decltype(m)>::handle;
            result[index].ordinal = ordinal;

            TRY_GET_ATTR(attrs, ReflectableBase::Attr_RequiredMember, a) {
              result[index].is_required = true;
            }
          }

          return true;
        });

    return result;
  }

  /**
   * @brief Compile-time sorted array of [string, handler] pairs.
   */
  const array_t dispatch_map{get_member_dispatch()};
};

struct StringLoaders {
  /**
   * @brief Loads single integer option value from a string
   *
   * @param value      String to load from
   * @param member     Member to load into
   */
  template <typename T>
  static typename std::enable_if<std::is_integral<T>::value, bool>::type
  load_impl(T& member, const char* value) {
    char* end = nullptr;

    if constexpr (std::is_unsigned<T>::value) {
      const auto result = std::strtoull(value, &end, 10);
      if (result > std::numeric_limits<T>::max()) {
        return false;
      }

      member = static_cast<T>(result);
    } else {
      const auto result = std::strtoll(value, &end, 10);
      if (result > std::numeric_limits<T>::max() ||
          result < std::numeric_limits<T>::min()) {
        return false;
      }

      member = static_cast<T>(result);
    }

    if (end == nullptr || *end != '\0') {
      return false;
    }

    return true;
  }

  /**
   * @brief Loads single enum option value from a string
   *
   * @param value      String to load from
   * @param member     Member to load into
   */
  template <typename T>
  static typename std::enable_if<!std::is_integral<T>::value &&
                                     std::is_enum<T>::value,
                                 bool>::type
  load_impl(T& member, const char* value) {
    typename std::underlying_type<T>::type parsed_value;
    RETURN_RESULT_IF_FAIL(load_impl(parsed_value, value));

    member = static_cast<T>(parsed_value);

    return true;
  }

  /**
   * @brief Loads single floating point option value from a string
   *
   * @param value      String to load from
   * @param member     Member to load into
   */
  template <typename T>
  static typename std::enable_if<std::is_floating_point<T>::value, bool>::type
  load_impl(T& member, const char* value) {
    double parsed_value;
    if (sscanf(value, "%lf", &parsed_value) != 1) {
      return false;
    }

    member = static_cast<T>(parsed_value);

    return true;
  }

  /**
   * @brief Serializes members that are convertible to and from string.
   */
  template <typename T>
  static typename std::enable_if<!std::is_integral<T>::value &&
                                     !std::is_enum<T>::value &&
                                     std::is_convertible<const char*, T>::value,
                                 bool>::type
  load_impl(T& member, const char* value) {
    member = value;
    return true;
  }

  template <typename T>
  struct is_supported_collection {
    static constexpr bool value =
        is_specialization_of<std::vector, T>::value ||
        is_specialization_of<std::deque, T>::value ||
        is_specialization_of<std::list, T>::value ||
        is_specialization_of<std::set, T>::value ||
        is_specialization_of<std::multiset, T>::value ||
        is_specialization_of<std::unordered_set, T>::value ||
        is_specialization_of<std::unordered_multiset, T>::value;
  };

  template <typename T>
  static typename std::enable_if<is_supported_collection<T>::value, bool>::type
  load_impl(T& member, const char* value) {
    typename T::value_type parsed_value;
    RETURN_RESULT_IF_FAIL(load_impl(parsed_value, value));

    if constexpr (is_specialization_of<std::vector, T>::value ||
                  is_specialization_of<std::deque, T>::value ||
                  is_specialization_of<std::list, T>::value) {
      member.emplace_back(parsed_value);
    } else {
      member.emplace(parsed_value);
    }

    return true;
  }

  /**
   * @brief Serializes std::optional<T> members.
   */
  template <typename T>
  static typename std::enable_if<!is_supported_collection<T>::value, bool>::type
  load_impl(std::optional<T>& member, const char* value) {
    T parsed_value{};
    RETURN_RESULT_IF_FAIL(load_impl(parsed_value, value));
    member = parsed_value;
    return true;
  }

  /**
   * @brief Serializes Reflectable members
   *
   * Ex: load_impl(setpoint, "fan_mode.1") results in fan_mode member of
   * setpoint being loaded from value 1 Ex: load_impl(state,
   * "setpoint.fan_mode.1") results in fan_mode member of setpoint member of
   * state being loaded from value 1.
   */
  template <typename T>
  static typename std::enable_if<is_reflectable<T>::value, bool>::type
  load_impl(T& member, const char* value) {
    const auto dot_offset = strchr(value, '.');
    if (dot_offset == nullptr) {
      return false;
    }
    const std::string_view prefix(value, dot_offset - value);
    const char* sub_value = dot_offset + 1;

    RETURN_RESULT_IF_FAIL(
        member.for_each_member_value([&](size_t ordinal, const char* name,
                                         auto& sub_member, const auto& attrs) {
          if constexpr (is_supported<decltype(member)>::value) {
            if (prefix == name) {
              return load_impl(sub_member, sub_value);
            }
          }

          return true;
        }));

    return true;
  }

  /**
   * `value` member is true if @see StringLoaders supports loading `U`
   *
   * @tparam  U      Type to check for support by @see StringLoaders
   */
  template <typename U>
  class is_supported {
   private:
    template <typename T, T>
    struct helper;
    template <typename T>
    static std::uint8_t check(helper<bool (*)(T&, const char*), &load_impl>*);
    template <typename T>
    static std::uint16_t check(...);

   public:
    static constexpr bool value = sizeof(check<U>(0)) == sizeof(std::uint8_t);
  };
};

/**
 * @brief Converts bool::REQUIRED_MISSING into true
 *
 * Useful when running multiple serializers in series when you only care about
 * required members after all serializers have run
 */
inline bool ignore_required(bool r) {
  return true;
}

}  // namespace ReflectLib::Impl
