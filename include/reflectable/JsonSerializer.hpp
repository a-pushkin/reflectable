#pragma once

#include "reflectable/Reflectable.hpp"
#include "reflectable/impl/SerializerCommon.hpp"

#include "nlohmann/json.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <deque>
#include <map>
#include <optional>
#include <queue>
#include <set>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>

namespace ReflectLib {

class JsonSerializer {
  using json = nlohmann::json;

  /**
   * @brief Serializes arithmetic members.
   */
  template <typename T>
  static typename std::enable_if<std::is_arithmetic<T>::value, bool>::type
  save_impl(json& j, const T& member) {
    j = member;
    return true;
  }

  /**
   * @brief Deserializes arithmetic members.
   */
  template <typename T>
  static typename std::enable_if<std::is_arithmetic<T>::value, bool>::type
  load_impl(const json& j, T& member) {
    member = j.get<T>();
    return true;
  }

  /**
   * @brief Serializes members that are convertible to and from string.
   */
  template <typename T>
  static typename std::enable_if<!std::is_arithmetic<T>::value &&
                                     std::is_enum<T>::value,
                                 bool>::type
  save_impl(json& j, const T& member) {
    const typename std::underlying_type<T>::type value{
        static_cast<typename std::underlying_type<T>::type>(member)};
    return save_impl(j, value);
  }

  /**
   * @brief Loads single enum option value from a string
   *
   * @param value      String to load from
   * @param member     Member to load into
   */
  template <typename T>
  static typename std::enable_if<!std::is_arithmetic<T>::value &&
                                     std::is_enum<T>::value,
                                 bool>::type
  load_impl(const json& j, T& member) {
    typename std::underlying_type<T>::type parsed_value;
    if (!load_impl(j, parsed_value)) {
      return false;
    }

    member = static_cast<T>(parsed_value);

    return true;
  }

  /**
   * @brief Serializes members that are convertible to and from string.
   */
  template <typename T>
  static
      typename std::enable_if<!std::is_arithmetic<T>::value &&
                                  !std::is_enum<T>::value &&
                                  std::is_convertible<T, std::string>::value &&
                                  std::is_convertible<std::string, T>::value,
                              bool>::type
      save_impl(json& j, const T& member) {
    j = static_cast<std::string>(member);
    return true;
  }

  /**
   * @brief Deserializes members that are convertible to and from string.
   */
  template <typename T>
  static
      typename std::enable_if<!std::is_arithmetic<T>::value &&
                                  !std::is_enum<T>::value &&
                                  std::is_convertible<T, std::string>::value &&
                                  std::is_convertible<std::string, T>::value,
                              bool>::type
      load_impl(const json& j, T& member) {
    member = static_cast<T>(j.get<std::string>());
    return true;
  }

  /**
   * @brief Serializes Reflectable<T, ...> members
   */
  template <typename T, typename TBase>
  static bool save_impl(json& j, const Reflectable<T, TBase>& member) {
    save(member, j);
    return true;
  }

  /**
   * @brief Deserializes Reflectable<T, ...> members
   */
  template <typename T, typename TBase>
  static bool load_impl(const json& j, Reflectable<T, TBase>& member) {
    return load(j, static_cast<T&>(member));
  }

  /**
   * @brief Serializers std::optional<T> members
   */
  template <typename T>
  static bool save_impl(json& j, const std::optional<T>& member) {
    if (member.has_value()) {
      if (!save_impl(j, member.value())) {
        return false;
      }
    } else {
      j = json(nullptr);
    }

    return true;
  }

  /**
   * @brief Deserializes std::optional<T> members
   */
  template <typename T>
  static bool load_impl(const json& j, std::optional<T>& member) {
    JsonSerializer ser;

    if (j.is_null()) {
      member.reset();
      return true;
    } else {
      T val;
      if (!load_impl(j, val)) {
        return false;
      }
      member = val;
      return true;
    }
  }

/* std::variant serialization is broken in clang */
#ifndef __clang__

  /**
   * @brief Serializers std::variant<T> members
   */
  template <typename... T>
  static bool save_impl(json& j, const std::variant<T...>& member) {
    j = json(2, member.index());

    //
    // TODO: Implement version that relies on get_type_name rather than index
    // Bonus points for selecting either functionality based on attribute
    ///
    bool result = false;
    std::visit([&](const auto& el) { result = save_impl(j[1], el); }, member);

    return result;
  }

  /**
   * @brief Deserializes std::variant<T...> members
   */
  template <typename... T>
  static bool load_impl(const json& j, std::variant<T...>& member) {
    JsonSerializer ser;

    if (!j.is_array() || j.size() != 2 || !j[0].is_number_unsigned()) {
      return false;
    }

    // TODO: This can be made much more efficient
    const size_t index = j[0].get<size_t>();
    size_t i = 0;
    bool result = false;
    std::tuple<T...> tuple;
    for_each_in_tuple(tuple, [&](auto t) {
      if (i++ == index) {
        result = load_impl(j[1], t);
        member = t;
      }
    });

    return result;
  }

#endif

  /**
   * @brief Serializes STL collections of known size
   */
  template <typename T>
  static typename std::enable_if<
      is_std_array<T>::value || is_specialization_of<std::vector, T>::value ||
          is_specialization_of<std::deque, T>::value ||
          is_specialization_of<std::set, T>::value ||
          is_specialization_of<std::multiset, T>::value ||
          is_specialization_of<std::unordered_set, T>::value ||
          is_specialization_of<std::unordered_multiset, T>::value,
      bool>::type
  save_impl(json& j, const T& member) {
    j = json(member.size(), nullptr);
    size_t i = 0;
    for (const auto& el : member) {
      if (!save_impl(j[i++], el)) {
        return false;
      }
    }

    return true;
  }

  /**
   * @brief Deserializes STL collections of known size
   */
  template <typename T>
  static typename std::enable_if<
      is_std_array<T>::value || is_specialization_of<std::vector, T>::value ||
          is_specialization_of<std::deque, T>::value ||
          is_specialization_of<std::set, T>::value ||
          is_specialization_of<std::multiset, T>::value ||
          is_specialization_of<std::unordered_set, T>::value ||
          is_specialization_of<std::unordered_multiset, T>::value,
      bool>::type
  load_impl(const json& j, T& member) {
    if (!j.is_array()) {
      return false;
    }

    if constexpr (is_std_array<T>::value) {
      if (j.size() != member.size()) {
        return false;
      }
    } else if constexpr (is_specialization_of<std::vector, T>::value) {
      member.resize(j.size());
    } else {
      member.clear();
    }

    for (size_t i = 0; i < j.size(); ++i) {
      if constexpr (is_std_array<T>::value ||
                    is_specialization_of<std::vector, T>::value) {
        if (!load_impl(j[i], member[i]) && false) {
          return false;
        }
      } else {
        typename T::value_type val;
        if (!load_impl(j[i], val) && false) {
          return false;
        }

        if constexpr (is_specialization_of<std::deque, T>::value) {
          member.push(val);
        } else {
          member.insert(val);
        }
      }
    }

    return true;
  }

  /**
   * @brief Serializes STL maps
   */
  template <typename T>
  static typename std::enable_if<
      is_specialization_of<std::map, T>::value ||
          is_specialization_of<std::multimap, T>::value ||
          is_specialization_of<std::unordered_map, T>::value ||
          is_specialization_of<std::unordered_multimap, T>::value,
      bool>::type
  save_impl(json& j, const T& member) {
    j = json(member.size(), nullptr);
    size_t i = 0;
    for (const auto& el : member) {
      auto& jj = j[i++] = json(2, nullptr);
      if (!save_impl(jj[0], el.first) || !save_impl(jj[1], el.second)) {
        return false;
      }
    }

    return true;
  }

  /**
   * @brief Deserializes STL maps
   */
  template <typename T>
  static typename std::enable_if<
      is_specialization_of<std::map, T>::value ||
          is_specialization_of<std::multimap, T>::value ||
          is_specialization_of<std::unordered_map, T>::value ||
          is_specialization_of<std::unordered_multimap, T>::value,
      bool>::type
  load_impl(const json& j, T& member) {
    if (!j.is_array()) {
      return false;
    }

    member.clear();
    for (const auto& jj : j) {
      if (!jj.is_array() || jj.size() != 2) {
        return false;
      }

      typename T::key_type key;
      typename T::mapped_type value;

      if (!load_impl(jj[0], key) || !load_impl(jj[1], value)) {
        return false;
      }

      member.emplace(key, value);
    }

    return true;
  }

  /**
   * @brief Serializes STL tuples
   */
  template <typename... T>
  static bool save_impl(json& j, const std::tuple<T...>& member) {
    j = json(sizeof...(T), nullptr);

    bool result = true;
    size_t i = 0;
    for_each_in_const_tuple(member, [&](const auto& el) {
      if (!result) {
        return;
      }

      result = save_impl(j[i++], el);
    });

    return result;
  }

  /**
   * @brief Deserializes STL tuples
   */
  template <typename... T>
  static bool load_impl(const json& j, std::tuple<T...>& member) {
    if (!j.is_array() || j.size() != sizeof...(T)) {
      return false;
    }

    bool result = true;
    size_t i = 0;
    for_each_in_tuple(member, [&](auto& el) {
      if (!result) {
        return;
      }

      result = load_impl(j[i++], el);
    });

    return result;
  }

  /**
   * @brief Serializes STL pairs
   */
  template <typename T1, typename T2>
  static bool save_impl(json& j, const std::pair<T1, T2>& member) {
    j = json(2, nullptr);

    if (!save_impl(j[0], member.first) || !save_impl(j[1], member.second)) {
      return false;
    }

    return true;
  }

  /**
   * @brief Deserializes STL pairs
   */
  template <typename T1, typename T2>
  static bool load_impl(const json& j, std::pair<T1, T2>& member) {
    if (!j.is_array() || j.size() != 2) {
      return false;
    }

    if (!load_impl(j[0], member.first) || !load_impl(j[1], member.second)) {
      return false;
    }

    return true;
  }

  /**
   * @brief Serializes chrono timepoints
   */
  template <typename TClock, typename TDuration>
  static bool save_impl(
      json& j,
      const std::chrono::time_point<TClock, TDuration>& member) {
    const auto us_since_epoch =
        (std::chrono::duration_cast<std::chrono::microseconds>(
             member -
             std::chrono::system_clock::time_point{
                 std::chrono::system_clock::duration{0}}))
            .count();

    return save_impl(j, us_since_epoch);
  }

  /**
   * @brief Deserializes chrono timepoints
   */
  template <typename TClock, typename TDuration>
  static bool load_impl(const json& j,
                        std::chrono::time_point<TClock, TDuration>& member) {
    uint64_t us_since_epoch{0};
    if (!load_impl(j, us_since_epoch)) {
      return false;
    }
    member =
        std::chrono::system_clock::time_point{
            std::chrono::system_clock::duration{0}} +
        std::chrono::microseconds{us_since_epoch};

    return true;
  }

  /**
   * @brief Serializes chrono durations
   */
  template <typename Rep, typename Ratio>
  static bool save_impl(json& j,
                        const std::chrono::duration<Rep, Ratio>& member) {
    const int64_t us =
        std::chrono::duration_cast<std::chrono::microseconds>(member).count();

    return save_impl(j, us);
  }

  /**
   * @brief Deserializes chrono durations
   */
  template <typename Rep, typename Ratio>
  static bool load_impl(const json& j,
                        std::chrono::duration<Rep, Ratio>& member) {
    int64_t us{0};
    if (!load_impl(j, us)) {
      return false;
    }
    member = std::chrono::duration_cast<std::chrono::duration<Rep, Ratio>>(
        std::chrono::microseconds{us});

    return true;
  }

 private:
  template <typename T>
  using handler_t = Impl::member_handler_t<T, const json&>;

  template <typename T, typename TMember>
  struct option_value_handler {
    static bool handle(T& t, const json& j) {
      auto r = load_impl(j, TMember::get(t));
      if (!r) {
        r = load_impl(j, TMember::get(t));
        return r;
      }

      return r;
    }
  };

  template <typename T>
  static bool load_json_impl(const json& source,
                             Impl::required_members_t<T>& required,
                             T& reflectable) {
    Impl::member_dispatch_t<T, handler_t<T>, option_value_handler, Attr_Ignore>
        long_dispatch{};

    if (!source.is_object()) {
      return false;
    }

    for (auto j = source.begin(); j != source.end(); ++j) {
      if (long_dispatch.find_string(j.key())) {
        if (!required.handle(long_dispatch.handler(), reflectable, j.value())) {
          return false;
        }
      }
    }

    return true;
  }

 public:
  struct Attr_Ignore {};

  /**
   * Loads Refelectable instance from URL query string
   */
  template <typename T>
  static bool load(const json& source,
                   Impl::required_members_t<T>& required,
                   T& reflectable) {
    return load_json_impl(source, required, reflectable);
  }

  template <typename T>
  static bool load(const json& source, T& reflectable) {
    Impl::required_members_t<T> required{};

    return load_json_impl(source, required, reflectable);
  }

  template <typename T>
  static void save(const Reflectable<T>& source, json& dest) {
    source.for_each_member_value([&](size_t ordinal, const char* name, auto m,
                                     const auto& attrs) -> bool {
      json j;
      save_impl(j, m);
      dest[name] = j;

      return true;
    });
  }
};

}  // namespace ReflectLib
