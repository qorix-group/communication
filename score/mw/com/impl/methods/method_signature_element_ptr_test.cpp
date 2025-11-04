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
#include "score/mw/com/impl/methods/method_signature_element_ptr.h"

#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{

struct TestElementType
{
    int value;
    explicit TestElementType(int v) : value(v) {}
};

class MethodSignatureElementPtrTestFixture : public ::testing::Test
{
  public:
    static constexpr int kTestElementValue = 42;
    static constexpr std::size_t kDefaultQueuePosition = 2;

  protected:
    MethodSignatureElementPtrTestFixture() = default;
    ~MethodSignatureElementPtrTestFixture() override = default;
    bool active_flag_{false};
    TestElementType test_element_{kTestElementValue};
};

TEST_F(MethodSignatureElementPtrTestFixture, Construction_SetsActiveFlag)
{
    // When constructing a MethodSignatureElementPtr with a TestElementType pointer and an active flag reference
    MethodSignatureElementPtr<TestElementType> ptr(test_element_, active_flag_, kDefaultQueuePosition);
    // Then the active flag is set to true after construction
    EXPECT_TRUE(active_flag_);
}

TEST_F(MethodSignatureElementPtrTestFixture, Construction_PointsToElement)
{
    // Given a MethodSignatureElementPtr constructed with a TestElementType pointer and an active flag reference
    MethodSignatureElementPtr<TestElementType> ptr(test_element_, active_flag_, kDefaultQueuePosition);
    // When calling get(), then the internal pointer points to the given TestElementType instance, the unit was
    // constructed with
    ASSERT_TRUE(ptr.get() != nullptr);
    EXPECT_EQ(ptr.get(), &test_element_);
}

TEST_F(MethodSignatureElementPtrTestFixture, Construction_CorrectQueuePosition)
{
    // Given a MethodSignatureElementPtr constructed with a TestElementType pointer and an active flag reference
    MethodSignatureElementPtr<TestElementType> ptr(test_element_, active_flag_, kDefaultQueuePosition);
    // when calling GetQueuePosition(), then the internal queue position is set correctly
    EXPECT_EQ(ptr.GetQueuePosition(), kDefaultQueuePosition);
}

TEST_F(MethodSignatureElementPtrTestFixture, Destruction_ClearsActiveFlag)
{
    {
        // Given a MethodSignatureElementPtr constructed with a TestElementType pointer and an active flag reference
        MethodSignatureElementPtr<TestElementType> ptr(test_element_, active_flag_, kDefaultQueuePosition);
        // Expect, that the active flag is set to true after construction
        EXPECT_TRUE(active_flag_);
    }
    // When the MethodSignatureElementPtr goes out of scope and is destroyed
    // then the active flag is set to false after destruction
    EXPECT_FALSE(active_flag_);
}

TEST_F(MethodSignatureElementPtrTestFixture, MoveConstruction_ActiveFlagSet_BeforeMovedFromInstanceDestroyed)
{
    MethodSignatureElementPtr<TestElementType> ptr(test_element_, active_flag_, kDefaultQueuePosition);

    // When move-constructing a new MethodSignatureElementPtr from another MethodSignatureElementPtr
    MethodSignatureElementPtr<TestElementType> ptr_from_temporary(std::move(ptr));

    // then the active flag is (still) set to true before the moved-from instance is destroyed
    EXPECT_TRUE(active_flag_);
}

TEST_F(MethodSignatureElementPtrTestFixture, MoveConstruction_ActiveFlagSet_AfterMovedFromInstanceDestroyed)
{
    std::optional<MethodSignatureElementPtr<TestElementType>> ptr_optional{};
    ptr_optional.emplace(test_element_, active_flag_, kDefaultQueuePosition);

    // Given that a new MethodSignatureElementPtr was move-constructed from another MethodSignatureElementPtr
    MethodSignatureElementPtr<TestElementType> ptr_from_temporary(std::move(ptr_optional.value()));

    // when destroying the moved-from instance
    ptr_optional.reset();
    // then the active flag is (still) set to true after the moved-from instance is destroyed
    EXPECT_TRUE(active_flag_);
}

TEST_F(MethodSignatureElementPtrTestFixture, MoveConstruction_ActiveFlagCleared_AfterMoveConstructedInstanceDestroyed)
{
    // Given a MethodSignatureElementPtr
    MethodSignatureElementPtr<TestElementType> ptr(test_element_, active_flag_, kDefaultQueuePosition);
    {
        // when we move construct a new MethodSignatureElementPtr from it
        MethodSignatureElementPtr<TestElementType> ptr_from_temporary(std::move(ptr));
    }
    // when the move-constructed instance is destroyed and the moved-from instance still exists, then the active flag is
    // false afterward
    EXPECT_FALSE(active_flag_);
}

TEST_F(MethodSignatureElementPtrTestFixture, MoveConstruction_CorrectQueuePosition)
{
    // Given a MethodSignatureElementPtr constructed with a TestElementType pointer and an active flag reference
    MethodSignatureElementPtr<TestElementType> ptr(test_element_, active_flag_, kDefaultQueuePosition);

    // When move-constructing a new MethodSignatureElementPtr from the first one
    MethodSignatureElementPtr<TestElementType> moved_ptr(std::move(ptr));
    // then the internal queue position is set correctly
    EXPECT_EQ(ptr.GetQueuePosition(), kDefaultQueuePosition);
}

TEST_F(MethodSignatureElementPtrTestFixture, MoveConstruction_CorrectElementValue)
{
    // Given a MethodSignatureElementPtr constructed with a TestElementType pointer and an active flag reference
    MethodSignatureElementPtr<TestElementType> ptr(test_element_, active_flag_, kDefaultQueuePosition);

    // When move-constructing a new MethodSignatureElementPtr from the first one
    MethodSignatureElementPtr<TestElementType> moved_ptr(std::move(ptr));
    // when calling get(), then the returned pointer points to the correct TestElementType instance
    EXPECT_EQ(moved_ptr.get(), &test_element_);
}

TEST_F(MethodSignatureElementPtrTestFixture, DereferenceOperator_WorksCorrectly)
{
    // Given a MethodSignatureElementPtr constructed with a TestElementType pointer and an active flag reference
    MethodSignatureElementPtr<TestElementType> ptr(test_element_, active_flag_, kDefaultQueuePosition);

    // When dereferencing the MethodSignatureElementPtr
    TestElementType& element_ref = *ptr;
    // then the returned reference points to the correct TestElementType instance
    EXPECT_EQ(element_ref.value, kTestElementValue);
}

TEST_F(MethodSignatureElementPtrTestFixture, ArrowOperator_WorksCorrectly)
{
    // Given a MethodSignatureElementPtr constructed with a TestElementType pointer and an active flag reference
    MethodSignatureElementPtr<TestElementType> ptr(test_element_, active_flag_, kDefaultQueuePosition);

    // When using the arrow operator on the MethodSignatureElementPtr
    // Then it returns the correct value
    EXPECT_EQ(ptr->value, kTestElementValue);
}

}  // namespace

}  // namespace score::mw::com::impl
