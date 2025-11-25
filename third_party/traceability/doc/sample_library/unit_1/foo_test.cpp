#include "third_party/traceability/doc/sample_library/unit_1/foo.h"

#include <gtest/gtest.h>

TEST(Foo, GetNumber)
{
    unit_1::Foo unit{};

    EXPECT_EQ(unit.GetNumber(), 42u);
}
