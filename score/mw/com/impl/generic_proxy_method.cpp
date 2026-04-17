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

#include "score/mw/com/impl/proxy_base.h"

#include <score/blank.hpp>

#include <cstddef>

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

score::Result<score::cpp::span<std::byte>> GenericProxyMethod::AllocateInArgs(
    const std::size_t queue_position) noexcept
{
    return binding_->AllocateInArgs(queue_position);
}

score::Result<score::cpp::span<std::byte>> GenericProxyMethod::AllocateReturnType(
    const std::size_t queue_position) noexcept
{
    return binding_->AllocateReturnType(queue_position);
}

ResultBlank GenericProxyMethod::Call(const std::size_t queue_position) noexcept
{
    return binding_->DoCall(queue_position);
}

}  // namespace score::mw::com::impl
