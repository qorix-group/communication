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
#include "score/mw/com/impl/bindings/lola/methods/type_erased_call_queue.h"

#include "score/memory/data_type_size_info.h"

#include "score/memory/shared/fake/my_bounded_memory_resource.h"
#include "score/memory/shared/memory_resource_proxy.h"
#include "score/memory/shared/pointer_arithmetic_util.h"

#include <score/assert_support.hpp>

#include <score/utility.hpp>
#include <gtest/gtest.h>
#include <cstddef>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

namespace score::mw::com::impl::lola
{
namespace
{

using namespace ::testing;

const std::size_t kQueueSize{9U};

constexpr memory::DataTypeSizeInfo kValidInArgTypeErasedInfo{104U, 8U};
const auto kExpectedInArgAllocationSize = kValidInArgTypeErasedInfo.Size() * kQueueSize;

constexpr memory::DataTypeSizeInfo kValidResultTypeErasedInfo{64U, 16U};
const auto kExpectedResultAllocationSize = kValidResultTypeErasedInfo.Size() * kQueueSize;

const std::size_t kExpectedPaddingBeweenInArgsAndResult{8U};

class TypeErasedCallQueueFixture : public ::testing::Test
{
  public:
    TypeErasedCallQueueFixture()
    {
        memory::shared::MemoryResourceProxy::EnableBoundsChecking(false);
    }

    TypeErasedCallQueueFixture& WithAnInArgTypeInfo()
    {
        type_erased_element_info_.in_arg_type_info = kValidInArgTypeErasedInfo;
        return *this;
    }

    TypeErasedCallQueueFixture& WithAResultTypeInfo()
    {
        type_erased_element_info_.result_type_info = kValidResultTypeErasedInfo;
        return *this;
    }

    TypeErasedCallQueueFixture& GivenATypeErasedCallQueue()
    {
        unit_ = std::make_unique<TypeErasedCallQueue>(*memory_resource_proxy_, type_erased_element_info_);
        return *this;
    }

    memory::shared::test::MyBoundedMemoryResource fake_memory_resource_{2000U};
    const memory::shared::MemoryResourceProxy* memory_resource_proxy_{fake_memory_resource_.getMemoryResourceProxy()};

    TypeErasedCallQueue::TypeErasedElementInfo type_erased_element_info_{{}, {}, kQueueSize};

    std::unique_ptr<TypeErasedCallQueue> unit_{nullptr};
};

using TypeErasedCallQueueAllocationFixture = TypeErasedCallQueueFixture;
TEST_F(TypeErasedCallQueueAllocationFixture, DoesNotAllocatesIfNoInArgTypeOrResultTypeInfoProvided)
{
    // When constructing a TypeErasedCallQueue with empty InArg and Result TypeInfos
    GivenATypeErasedCallQueue();

    // Then no memory should have been allocated
    EXPECT_EQ(fake_memory_resource_.GetUserAllocatedBytes(), 0U);
}

TEST_F(TypeErasedCallQueueAllocationFixture, AllocatesInArgOnConstructionIfInArgTypeInfoProvided)
{
    // When constructing a TypeErasedCallQueue with only an InArg TypeInfo
    WithAnInArgTypeInfo().GivenATypeErasedCallQueue();

    // Then memory should have been allocated for the all in args in the queue taking into account element padding
    EXPECT_EQ(fake_memory_resource_.GetUserAllocatedBytes(), kExpectedInArgAllocationSize);
}

TEST_F(TypeErasedCallQueueAllocationFixture, AllocatesResultOnConstructionIfResultTypeInfoProvided)
{
    // When constructing a TypeErasedCallQueue with only an Result TypeInfo
    WithAResultTypeInfo().GivenATypeErasedCallQueue();

    // Then memory should have been allocated for the all results in the queue taking into account element padding
    EXPECT_EQ(fake_memory_resource_.GetUserAllocatedBytes(), kExpectedResultAllocationSize);
}

TEST_F(TypeErasedCallQueueAllocationFixture, AllocatesResultAndInArgsOnConstructionIfResultAndInArgTypeInfosProvided)
{
    // When constructing a TypeErasedCallQueue with both InArg and result TypeInfos
    WithAnInArgTypeInfo().WithAResultTypeInfo().GivenATypeErasedCallQueue();

    // Then memory should have been allocated for the all the in args and results in the queue
    const auto total_allocation_size =
        kExpectedInArgAllocationSize + kExpectedPaddingBeweenInArgsAndResult + kExpectedResultAllocationSize;
    EXPECT_EQ(fake_memory_resource_.GetUserAllocatedBytes(), total_allocation_size);
}

TEST_F(TypeErasedCallQueueAllocationFixture, DoesNotDeallocateIfNoTypeInfosProvided)
{
    GivenATypeErasedCallQueue();

    // When destroying the TypeErasedCallQueue
    unit_.reset();

    // Then the no memory should be deallocated
    EXPECT_EQ(fake_memory_resource_.GetUserDeAllocatedBytes(), 0U);
}

TEST_F(TypeErasedCallQueueAllocationFixture, DeallocatesInArgsOnDestructionIfInArgTypeInfoProvided)
{
    WithAnInArgTypeInfo().GivenATypeErasedCallQueue();

    // When destroying the TypeErasedCallQueue
    unit_.reset();

    // Then the memory that was allocated for the InArgs should have been deallocated
    EXPECT_EQ(fake_memory_resource_.GetUserDeAllocatedBytes(), kExpectedInArgAllocationSize);
}

TEST_F(TypeErasedCallQueueAllocationFixture, DeallocatesResultOnDestructionIfResultTypeInfoProvided)
{
    WithAResultTypeInfo().GivenATypeErasedCallQueue();

    // When destroying the TypeErasedCallQueue
    unit_.reset();

    // Then the memory that was allocated for the Result should have been deallocated
    EXPECT_EQ(fake_memory_resource_.GetUserDeAllocatedBytes(), kExpectedResultAllocationSize);
}

TEST_F(TypeErasedCallQueueAllocationFixture, DeallocatesInArgsAndResultOnDestructionIfInArgAndResultTypeInfosProvided)
{
    WithAnInArgTypeInfo().WithAResultTypeInfo().GivenATypeErasedCallQueue();

    // When destroying the TypeErasedCallQueue
    unit_.reset();

    // Then the memory that was allocated for the InArgs and Result should have been deallocated
    const auto expected_deallocation_size = kExpectedInArgAllocationSize + kExpectedResultAllocationSize;
    EXPECT_EQ(fake_memory_resource_.GetUserDeAllocatedBytes(), expected_deallocation_size);
}

TEST_F(TypeErasedCallQueueFixture, GetInArgsStoragePointsToCorrectPositionInQueue)
{
    WithAnInArgTypeInfo().GivenATypeErasedCallQueue();

    // When calling GetInArgsStorage with the index of every element in the queue
    auto* const queue_start_address = fake_memory_resource_.getUsableBaseAddress();
    for (std::size_t i = 0; i < kQueueSize; ++i)
    {
        auto in_args_storage = unit_->GetInArgsStorage(i);

        // Then the returned storage should have the correct address and size
        ASSERT_TRUE(in_args_storage.has_value());
        auto* const expected_element_address =
            memory::shared::AddOffsetToPointer(queue_start_address, i * kValidInArgTypeErasedInfo.Size());
        EXPECT_EQ(in_args_storage.value().data(), expected_element_address);
        EXPECT_EQ(in_args_storage.value().size(), kValidInArgTypeErasedInfo.Size());
    }
}

TEST_F(TypeErasedCallQueueFixture, GetInArgsStoragePointsToCorrectPositionInQueueWithInArgsAndResultTypeInfos)
{
    WithAnInArgTypeInfo().WithAResultTypeInfo().GivenATypeErasedCallQueue();

    // When calling GetInArgsStorage with the index of every element in the queue
    auto* const queue_start_address = fake_memory_resource_.getUsableBaseAddress();
    for (std::size_t i = 0; i < kQueueSize; ++i)
    {
        auto in_args_storage = unit_->GetInArgsStorage(i);

        // Then the returned storage should have the correct address and size
        ASSERT_TRUE(in_args_storage.has_value());
        auto* const expected_element_address =
            memory::shared::AddOffsetToPointer(queue_start_address, i * kValidInArgTypeErasedInfo.Size());
        EXPECT_EQ(in_args_storage.value().data(), expected_element_address);
        EXPECT_EQ(in_args_storage.value().size(), kValidInArgTypeErasedInfo.Size());
    }
}

TEST_F(TypeErasedCallQueueFixture, GetResultStoragePointsToCorrectPositionInQueue)
{
    WithAResultTypeInfo().GivenATypeErasedCallQueue();

    // When calling GetInArgsStorage with the index of every element in the queue
    auto* const queue_start_address = fake_memory_resource_.getUsableBaseAddress();
    for (std::size_t i = 0; i < kQueueSize; ++i)
    {
        auto results_storage = unit_->GetResultStorage(i);

        // Then the returned storage should have the correct address and size
        ASSERT_TRUE(results_storage.has_value());
        auto* const expected_element_address =
            memory::shared::AddOffsetToPointer(queue_start_address, i * kValidResultTypeErasedInfo.Size());
        EXPECT_EQ(results_storage.value().data(), expected_element_address);
        EXPECT_EQ(results_storage.value().size(), kValidResultTypeErasedInfo.Size());
    }
}

TEST_F(TypeErasedCallQueueFixture, GetResultStoragePointsToCorrectPositionInQueueWithInArgsAndResultTypeInfos)
{
    WithAnInArgTypeInfo().WithAResultTypeInfo().GivenATypeErasedCallQueue();

    // When calling GetInArgsStorage with the index of every element in the queue
    const auto expected_in_arg_allocation_size_plus_padding =
        kValidInArgTypeErasedInfo.Size() * kQueueSize + kExpectedPaddingBeweenInArgsAndResult;
    auto* const result_queue_start_address = memory::shared::AddOffsetToPointer(
        fake_memory_resource_.getUsableBaseAddress(), expected_in_arg_allocation_size_plus_padding);
    for (std::size_t i = 0; i < kQueueSize; ++i)
    {
        auto results_storage = unit_->GetResultStorage(i);

        // Then the returned storage should have the correct address and size
        ASSERT_TRUE(results_storage.has_value());
        auto* const expected_element_address =
            memory::shared::AddOffsetToPointer(result_queue_start_address, i * kValidResultTypeErasedInfo.Size());
        EXPECT_EQ(results_storage.value().data(), expected_element_address);
        EXPECT_EQ(results_storage.value().size(), kValidResultTypeErasedInfo.Size());
    }
}

TEST_F(TypeErasedCallQueueFixture, GetInArgPtrWithoutProvidingInArgTypeInfoReturnsEmptyOptional)
{
    WithAResultTypeInfo().GivenATypeErasedCallQueue();

    // When calling GetInArgsStorage with the index of every element in the queue
    for (std::size_t i = 0; i < kQueueSize; ++i)
    {
        auto in_args_storage = unit_->GetInArgsStorage(i);

        // Then an empty optional should be returned
        EXPECT_FALSE(in_args_storage.has_value());
    }
}

TEST_F(TypeErasedCallQueueFixture, GetResultStorageWithoutProvidingResultTypeInfoReturnsEmptyOptional)
{
    WithAnInArgTypeInfo().GivenATypeErasedCallQueue();

    // When calling GetInArgsStorage with the index of every element in the queue
    for (std::size_t i = 0; i < kQueueSize; ++i)
    {
        auto results_storage = unit_->GetResultStorage(i);

        // Then an empty optional should be returned
        EXPECT_FALSE(results_storage.has_value());
    }
}

TEST_F(TypeErasedCallQueueFixture, GetInArgsStorageWithOutOfRangePositionTerminates)
{
    WithAnInArgTypeInfo().WithAResultTypeInfo().GivenATypeErasedCallQueue();

    // When calling GetInArgsStorage with an index which is out of the queue range
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = unit_->GetInArgsStorage(kQueueSize));
}

TEST_F(TypeErasedCallQueueFixture, GetResultStorageWithOutOfRangePositionTerminates)
{
    WithAnInArgTypeInfo().WithAResultTypeInfo().GivenATypeErasedCallQueue();

    // When calling GetResultStorage with an index which is out of the queue range
    // Then the program terminates
    SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED(score::cpp::ignore = unit_->GetResultStorage(kQueueSize));
}

}  // namespace
}  // namespace score::mw::com::impl::lola
