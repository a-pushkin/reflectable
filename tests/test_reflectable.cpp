#include <gtest/gtest.h>
#include "reflectable/Reflectable.hpp"

using namespace ReflectLib;

namespace {

struct TwoMember : public Reflectable<TwoMember> {
  BEGIN_REFLECTABLE_MEMBERS()
  REFLECTABLE_MEMBER(int, foo)
  REFLECTABLE_MEMBER(float, bar)
  END_REFLECTABLE_MEMBERS()
};

TEST(Reflectable, is_reflectable) {
  struct Foo {};

  EXPECT_FALSE(is_reflectable<Foo>());
  EXPECT_TRUE(is_reflectable<TwoMember>());
}

TEST(Reflectable, count) {
  EXPECT_EQ(2U, reflectable_count<TwoMember>());
}

}  // namespace