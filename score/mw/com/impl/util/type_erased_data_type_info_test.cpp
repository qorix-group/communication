/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#include "score/mw/com/impl/util/type_erased_data_type_info.h"
#include "score/mw/com/impl/util/type_erased_storage.h"

#include <gtest/gtest.h>

#include <cstddef>

namespace score::mw::com::impl
{
namespace
{

constexpr std::size_t kDummySize{10U};
constexpr std::size_t kDummyAlignment{20U};

TEST(TypeErasedDataTypeInfoEqualityTest, ObjectsWithSameSizeAndAlignmentCompareTrue)
{
    // Given two TypeErasedDataTypeInfo objects with the same size and alignment
    TypeErasedDataTypeInfo unit{kDummySize, kDummyAlignment};
    TypeErasedDataTypeInfo unit2{kDummySize, kDummyAlignment};

    // When comparing the two objects
    const auto compare_result = unit == unit2;

    // Then the result should be true
    EXPECT_TRUE(compare_result);
}

TEST(TypeErasedDataTypeInfoEqualityTest, ObjectsWithDifferentSizeCompareFalse)
{
    // Given two TypeErasedDataTypeInfo objects with the different sizes
    TypeErasedDataTypeInfo unit{kDummySize, kDummyAlignment};
    TypeErasedDataTypeInfo unit2{kDummySize + 1U, kDummyAlignment};

    // When comparing the two objects
    const auto compare_result = unit == unit2;

    // Then the result should be false
    EXPECT_FALSE(compare_result);
}

TEST(TypeErasedDataTypeInfoEqualityTest, ObjectsWithDifferentAlignemtCompareFalse)
{
    // Given two TypeErasedDataTypeInfo objects with the different alignments
    TypeErasedDataTypeInfo unit{kDummySize, kDummyAlignment};
    TypeErasedDataTypeInfo unit2{kDummySize, kDummyAlignment + 1U};

    // When comparing the two objects
    const auto compare_result = unit == unit2;

    // Then the result should be false
    EXPECT_FALSE(compare_result);
}

}  // namespace
}  // namespace score::mw::com::impl
