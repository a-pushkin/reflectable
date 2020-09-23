#include <gtest/gtest.h>
#include <map>
#include "reflectable/JsonSerializer.hpp"

using namespace ReflectLib;
using namespace nlohmann;

namespace {

struct TwoMember : public Reflectable<TwoMember> {
  static constexpr int default_foo = 42;
  static constexpr float default_bar = 1.1;

  BEGIN_REFLECTABLE_MEMBERS()
  REFLECTABLE_MEMBER(int, foo, default_foo)
  REFLECTABLE_MEMBER(float, bar, default_bar)
  REFLECTABLE_MEMBER((std::map<int, float>), baz)
  END_REFLECTABLE_MEMBERS()
};

TEST(JsonSerializer, load_from_empty) {
  TwoMember config;
  json empty = json::object();

  ASSERT_TRUE(JsonSerializer::load(empty, config));

  EXPECT_EQ(config.foo, TwoMember::default_foo);
  EXPECT_EQ(config.bar, TwoMember::default_bar);
}

TEST(JsonSerializer, load_from_partial) {
  TwoMember config;
  json partial = json::object();
  constexpr int test_foo = 1;
  partial["foo"] = test_foo;

  ASSERT_TRUE(JsonSerializer::load(partial, config));

  EXPECT_EQ(config.foo, test_foo);
  EXPECT_EQ(config.bar, TwoMember::default_bar);
}

struct Nested : public Reflectable<Nested> {
  BEGIN_REFLECTABLE_MEMBERS()
  REFLECTABLE_MEMBER(TwoMember, nested)
  END_REFLECTABLE_MEMBERS()
};

TEST(JsonSerializer, load_nested) {
  Nested config;
  json partial = json::object();

  constexpr int test_foo = 1;
  partial["nested"]["foo"] = test_foo;

  ASSERT_TRUE(JsonSerializer::load(partial, config));

  EXPECT_EQ(config.nested.foo, test_foo);
  EXPECT_EQ(config.nested.bar, TwoMember::default_bar);
}

}  // namespace