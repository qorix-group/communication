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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_I_TYPE_ERASED_CALL_QUEUE_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_I_TYPE_ERASED_CALL_QUEUE_H

#include "score/memory/data_type_size_info.h"

#include <score/span.hpp>

#include <cstddef>
#include <optional>

namespace score::mw::com::impl::lola
{

/// \brief Interface for class which manages the memory required for the InTypes and ReturnType for a specific service
/// Method
///
/// We provide an interface so that mocks can be used to test higher layers such as ProxyMethod / SkeletonMethod.
/// In production, the TypeErasedCallQueues are stored within MethodData in shared memory. Since absolute
/// pointers cannot be used in shared memory, we cannot use V-tables and therefore MethodData must NOT store pointers to
/// this interface and rely on runtime polymorphism, but should own TypeErasedCallQueues directly. A ProxyMethod /
/// SkeletonMethod does not reside in shared memory and therefore it's safe to inject ITypeErasedCallQueue into those
/// and rely on runtime polymorphism.
class ITypeErasedCallQueue
{
  public:
    struct TypeErasedElementInfo
    {
        std::optional<memory::DataTypeSizeInfo> in_arg_type_info;
        std::optional<memory::DataTypeSizeInfo> return_type_info;
        std::size_t queue_size;
    };

    ITypeErasedCallQueue() = default;
    virtual ~ITypeErasedCallQueue() = default;

    ITypeErasedCallQueue(const ITypeErasedCallQueue&) = delete;
    ITypeErasedCallQueue& operator=(const ITypeErasedCallQueue&) = delete;
    ITypeErasedCallQueue(ITypeErasedCallQueue&&) noexcept = delete;
    ITypeErasedCallQueue& operator=(ITypeErasedCallQueue&&) noexcept = delete;

    virtual std::optional<score::cpp::span<std::byte>> GetInArgValuesQueueStorage() const = 0;

    virtual std::optional<score::cpp::span<std::byte>> GetReturnValueQueueStorage() const = 0;

    virtual const TypeErasedElementInfo& GetTypeErasedElementInfo() const = 0;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_I_TYPE_ERASED_CALL_QUEUE_H
