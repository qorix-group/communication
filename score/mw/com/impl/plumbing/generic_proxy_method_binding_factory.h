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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_GENERIC_PROXY_METHOD_BINDING_FACTORY_H_
#define SCORE_MW_COM_IMPL_PLUMBING_GENERIC_PROXY_METHOD_BINDING_FACTORY_H_

#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/methods/proxy_method_binding.h"
#include "score/mw/com/impl/proxy_binding.h"

#include "score/memory/data_type_size_info.h"

#include <memory>
#include <optional>
#include <string_view>

namespace score::mw::com::impl
{

/// @brief Non-template factory for creating ProxyMethodBinding instances with runtime-provided type size information.
///
/// Mirrors ProxyMethodBindingFactory<Sig> but accepts DataTypeSizeInfo at runtime instead of computing
/// it from template type parameters. Used by GenericProxy to create GenericProxyMethod bindings.
class GenericProxyMethodBindingFactory final
{
  public:
    /// @brief Size information for the method's in-arguments and return type.
    struct MethodSizeInfo
    {
        std::optional<memory::DataTypeSizeInfo> in_args{std::nullopt};
        std::optional<memory::DataTypeSizeInfo> return_type{std::nullopt};
    };

    /// @brief Creates a ProxyMethodBinding for the given method using runtime size info.
    /// @param parent_handle  Handle of the parent proxy (provides deployment info).
    /// @param parent_binding Binding of the parent proxy (provides the LoLa proxy pointer).
    /// @param method_name    Name of the method in the service deployment.
    /// @param size_info      Size/alignment of the method's in-args and return type.
    /// @return A ProxyMethodBinding, or nullptr if the binding could not be created.
    static std::unique_ptr<ProxyMethodBinding> Create(HandleType parent_handle,
                                                      ProxyBinding* parent_binding,
                                                      std::string_view method_name,
                                                      MethodSizeInfo size_info) noexcept;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_GENERIC_PROXY_METHOD_BINDING_FACTORY_H_
