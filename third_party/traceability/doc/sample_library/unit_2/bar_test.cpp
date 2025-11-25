#include "third_party/traceability/doc/sample_library/unit_2/bar.h"

#include <gtest/gtest.h>

TEST(Bar, AssertNumber)
{
    ::testing::Test::RecordProperty("lobster-tracing", "SampleLibrary.SampleRequirement");
    unit_2::Bar unit{std::make_unique<unit_1::Foo>()};

    EXPECT_TRUE(unit.AssertNumber());
}
