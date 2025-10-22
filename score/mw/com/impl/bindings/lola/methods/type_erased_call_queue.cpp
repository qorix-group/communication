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

#include "score/memory/shared/memory_resource_proxy.h"
#include "score/memory/shared/offset_ptr.h"
#include "score/memory/shared/pointer_arithmetic_util.h"

#include "score/mw/com/impl/bindings/lola/methods/i_type_erased_call_queue.h"
#include <score/assert.hpp>

#include <cstddef>
#include <optional>
#include <tuple>

namespace score::mw::com::impl::lola
{

TypeErasedCallQueue::TypeErasedCallQueue(const memory::shared::MemoryResourceProxy& resource_proxy,
                                         const TypeErasedElementInfo& type_erased_element_info)
    : ITypeErasedCallQueue(),
      resource_proxy_{resource_proxy},
      type_erased_element_info_{type_erased_element_info},
      in_args_queue_start_address_{nullptr},
      result_queue_start_address_{nullptr}
{
    // If we have neither InArgs nor a Result, then we don't need to allocate any memory at all.
    if (!(type_erased_element_info_.in_arg_type_info.has_value() ||
          type_erased_element_info_.result_type_info.has_value()))
    {
        return;
    }
    std::tie(in_args_queue_start_address_, result_queue_start_address_) = AllocateQueue();
}

TypeErasedCallQueue::~TypeErasedCallQueue()
{
    if (in_args_queue_start_address_ != nullptr)
    {
        const auto& in_arg_type_info = type_erased_element_info_.in_arg_type_info.value();
        const auto allocated_size = in_arg_type_info.Size() * type_erased_element_info_.queue_size;
        resource_proxy_.deallocate(in_args_queue_start_address_.get(), allocated_size);
    }
    if (result_queue_start_address_ != nullptr)
    {
        const auto& result_type_info = type_erased_element_info_.result_type_info.value();
        const auto allocated_size = result_type_info.Size() * type_erased_element_info_.queue_size;
        resource_proxy_.deallocate(result_queue_start_address_.get(), allocated_size);
    }
}

std::optional<score::cpp::span<std::byte>> TypeErasedCallQueue::GetInArgsStorage(const size_t position) const
{
    if (!(type_erased_element_info_.in_arg_type_info.has_value()))
    {
        return {};
    }
    return GetElement(position,
                      type_erased_element_info_.in_arg_type_info.value(),
                      in_args_queue_start_address_,
                      type_erased_element_info_.queue_size);
}

std::optional<score::cpp::span<std::byte>> TypeErasedCallQueue::GetResultStorage(const size_t position) const
{
    if (!(type_erased_element_info_.result_type_info.has_value()))
    {
        return {};
    }
    return GetElement(position,
                      type_erased_element_info_.result_type_info.value(),
                      result_queue_start_address_,
                      type_erased_element_info_.queue_size);
}

std::pair<memory::shared::OffsetPtr<std::byte>, memory::shared::OffsetPtr<std::byte>>
TypeErasedCallQueue::AllocateQueue() const
{
    std::byte* in_args_queue_start_address_bytes{nullptr};
    if (type_erased_element_info_.in_arg_type_info.has_value())
    {
        const auto& in_arg_type_info = type_erased_element_info_.in_arg_type_info.value();
        const auto required_size = in_arg_type_info.Size() * type_erased_element_info_.queue_size;
        void* const in_args_queue_start_address = resource_proxy_.allocate(required_size, in_arg_type_info.Alignment());
        in_args_queue_start_address_bytes = static_cast<std::byte*>(in_args_queue_start_address);
    }

    std::byte* result_queue_start_address_bytes{nullptr};
    if (type_erased_element_info_.result_type_info.has_value())
    {
        const auto& result_type_info = type_erased_element_info_.result_type_info.value();
        const auto required_size = result_type_info.Size() * type_erased_element_info_.queue_size;
        void* const result_queue_start_address = resource_proxy_.allocate(required_size, result_type_info.Alignment());
        result_queue_start_address_bytes = static_cast<std::byte*>(result_queue_start_address);
    }

    return {in_args_queue_start_address_bytes, result_queue_start_address_bytes};
}

score::cpp::span<std::byte> TypeErasedCallQueue::GetElement(const std::size_t position,
                                                     const memory::DataTypeSizeInfo& type_info,
                                                     memory::shared::OffsetPtr<std::byte> queue_storage,
                                                     const std::size_t queue_size)
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD(position < queue_size);

    const auto element_offset = type_info.Size() * position;
    auto* const element_address = memory::shared::AddOffsetToPointer(queue_storage.get(), element_offset);

    return score::cpp::span{element_address, type_info.Size()};
}

}  // namespace score::mw::com::impl::lola
