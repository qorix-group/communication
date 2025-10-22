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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_TYPE_ERASED_CALL_QUEUE_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_TYPE_ERASED_CALL_QUEUE_H

#include "score/memory/data_type_size_info.h"
#include "score/mw/com/impl/bindings/lola/methods/i_type_erased_call_queue.h"

#include "score/memory/shared/memory_resource_proxy.h"
#include "score/memory/shared/offset_ptr.h"

#include <score/span.hpp>

#include <cstddef>
#include <optional>
#include <tuple>

namespace score::mw::com::impl::lola
{

/// \brief Class which manages the memory required for the InTypes and ResultType for a specific service Method
///
/// This class allocates the memory region for the call queue on construction and deallocates it on destruction.
class TypeErasedCallQueue final : public ITypeErasedCallQueue
{
  public:
    struct TypeErasedElementInfo
    {
        std::optional<memory::DataTypeSizeInfo> in_arg_type_info;
        std::optional<memory::DataTypeSizeInfo> result_type_info;
        std::size_t queue_size;
    };

    TypeErasedCallQueue(const memory::shared::MemoryResourceProxy& resource_proxy,
                        const TypeErasedElementInfo& type_erased_element_info);

    ~TypeErasedCallQueue() final;

    TypeErasedCallQueue(const TypeErasedCallQueue&) = delete;
    TypeErasedCallQueue& operator=(const TypeErasedCallQueue&) = delete;
    TypeErasedCallQueue(TypeErasedCallQueue&&) noexcept = delete;
    TypeErasedCallQueue& operator=(TypeErasedCallQueue&&) noexcept = delete;

    std::optional<score::cpp::span<std::byte>> GetInArgsStorage(size_t position) const override;

    std::optional<score::cpp::span<std::byte>> GetResultStorage(size_t position) const override;

  private:
    std::pair<memory::shared::OffsetPtr<std::byte>, memory::shared::OffsetPtr<std::byte>> AllocateQueue() const;

    static score::cpp::span<std::byte> GetElement(const std::size_t position,
                                           const memory::DataTypeSizeInfo& type_info,
                                           memory::shared::OffsetPtr<std::byte> queue_storage,
                                           const std::size_t queue_size);

    const memory::shared::MemoryResourceProxy& resource_proxy_;

    TypeErasedElementInfo type_erased_element_info_;

    memory::shared::OffsetPtr<std::byte> in_args_queue_start_address_;
    memory::shared::OffsetPtr<std::byte> result_queue_start_address_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_TYPE_ERASED_CALL_QUEUE_H
