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
#include "score/mw/com/impl/generic_proxy_method.h"

#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/proxy_base.h"

#include <score/blank.hpp>
#include <cstring>

namespace score::mw::com::impl
{

GenericProxyMethod::GenericProxyMethod(ProxyBase& proxy_base,
                                       const std::string_view method_name,
                                       std::unique_ptr<ProxyMethodBinding> binding)
    : ProxyMethodBase(proxy_base, std::move(binding), method_name)
{
    ProxyBaseView{proxy_base}.RegisterMethod(method_name_, *this);
}

GenericProxyMethod::GenericProxyMethod(GenericProxyMethod&& other) noexcept : ProxyMethodBase(std::move(other))
{
    ProxyBaseView{proxy_base_.get()}.UpdateMethod(method_name_, *this);
}

GenericProxyMethod& GenericProxyMethod::operator=(GenericProxyMethod&& other) & noexcept
{
    if (this != &other)
    {
        ProxyMethodBase::operator=(std::move(other));
        ProxyBaseView{proxy_base_.get()}.UpdateMethod(method_name_, *this);
    }
    return *this;
}

ResultBlank GenericProxyMethod::Call(const score::cpp::span<const std::byte> in_args,
                                     const score::cpp::span<std::byte> return_buf) noexcept
{
    constexpr std::size_t kQueuePos{0U};

    if (is_return_type_ptr_active_[kQueuePos])
    {
        return MakeUnexpected(ComErrc::kMaxSamplesReached);
    }

    if (!in_args.empty())
    {
        auto alloc_result = binding_->AllocateInArgs(kQueuePos);
        if (!alloc_result.has_value())
        {
            return MakeUnexpected<score::cpp::blank>(alloc_result.error());
        }
        // NOLINTNEXTLINE(score-banned-function)
        std::memcpy(alloc_result.value().data(), in_args.data(), in_args.size());
    }

    if (!return_buf.empty())
    {
        auto alloc_result = binding_->AllocateReturnType(kQueuePos);
        if (!alloc_result.has_value())
        {
            return MakeUnexpected<score::cpp::blank>(alloc_result.error());
        }

        is_return_type_ptr_active_[kQueuePos] = true;
        const auto call_result = binding_->DoCall(kQueuePos);
        is_return_type_ptr_active_[kQueuePos] = false;

        if (!call_result.has_value())
        {
            return call_result;
        }
        // NOLINTNEXTLINE(score-banned-function)
        std::memcpy(return_buf.data(), alloc_result.value().data(), return_buf.size());
    }
    else
    {
        const auto call_result = binding_->DoCall(kQueuePos);
        if (!call_result.has_value())
        {
            return call_result;
        }
    }

    return {};
}

}  // namespace score::mw::com::impl
