#include <gtest/gtest.h>
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
  END_REFLECTABLE_MEMBERS()
};

TEST(JsonSerializer, load_from_empty) {
  TwoMember config;
  json empty{};

  ASSERT_TRUE(JsonSerializer::load(empty, config));

  EXPECT_EQ(config.foo, TwoMember::default_foo);
  EXPECT_EQ(config.bar, TwoMember::default_bar);
}

}  // namespace