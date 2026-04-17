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
#ifndef SCORE_MW_COM_IMPL_GENERIC_PROXY_METHOD_H_
#define SCORE_MW_COM_IMPL_GENERIC_PROXY_METHOD_H_

#include "score/mw/com/impl/methods/proxy_method_base.h"
#include "score/mw/com/impl/methods/proxy_method_binding.h"
#include "score/result/result.h"

#include <score/span.hpp>

#include <cstddef>
#include <memory>
#include <string_view>

namespace score::mw::com::impl
{

class ProxyBase;

/// @brief Type-erased, non-templated counterpart of ProxyMethod<Sig>.
///
/// Invokes a remote method by passing in-args and receiving the return value as raw byte spans,
/// without any compile-time knowledge of argument or return types. Intended for use by GenericProxy.
class GenericProxyMethod final : public ProxyMethodBase
{
  public:
    GenericProxyMethod(ProxyBase& proxy_base,
                       std::string_view method_name,
                       std::unique_ptr<ProxyMethodBinding> binding);

    ~GenericProxyMethod() = default;

    GenericProxyMethod(const GenericProxyMethod&) = delete;
    GenericProxyMethod& operator=(const GenericProxyMethod&) & = delete;

    GenericProxyMethod(GenericProxyMethod&& other) noexcept;
    GenericProxyMethod& operator=(GenericProxyMethod&& other) & noexcept;

    score::Result<score::cpp::span<std::byte>> AllocateInArgs(std::size_t queue_position) noexcept;
    score::Result<score::cpp::span<std::byte>> AllocateReturnType(std::size_t queue_position) noexcept;
    ResultBlank                                Call(std::size_t queue_position) noexcept;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_GENERIC_PROXY_METHOD_H_
