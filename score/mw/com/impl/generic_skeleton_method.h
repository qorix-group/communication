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
#ifndef SCORE_MW_COM_IMPL_GENERIC_SKELETON_METHOD_H_
#define SCORE_MW_COM_IMPL_GENERIC_SKELETON_METHOD_H_

#include "score/mw/com/impl/methods/skeleton_method_base.h"
#include "score/mw/com/impl/methods/skeleton_method_binding.h"
#include "score/result/result.h"

#include <memory>
#include <string_view>

namespace score::mw::com::impl
{

class SkeletonBase;

/// @brief Type-erased, non-templated counterpart of SkeletonMethod<Sig>.
///
/// Receives incoming method calls as raw byte spans via a registered
/// TypeErasedHandler, without any compile-time knowledge of argument or
/// return types. Intended for use by GenericSkeleton.
class GenericSkeletonMethod final : public SkeletonMethodBase
{
  public:
    GenericSkeletonMethod(SkeletonBase& skeleton_base,
                          std::string_view method_name,
                          std::unique_ptr<SkeletonMethodBinding> binding);

    ~GenericSkeletonMethod() = default;

    GenericSkeletonMethod(const GenericSkeletonMethod&) = delete;
    GenericSkeletonMethod& operator=(const GenericSkeletonMethod&) & = delete;

    GenericSkeletonMethod(GenericSkeletonMethod&& other) noexcept;
    GenericSkeletonMethod& operator=(GenericSkeletonMethod&& other) & noexcept;

    /// @brief Register a handler that receives in-args and return-value buffers as raw byte spans.
    ///
    /// The binding invokes the handler on each incoming call. The caller is responsible for
    /// interpreting the spans according to the DataTypeMetaInfo configured at skeleton creation.
    ResultBlank RegisterHandler(SkeletonMethodBinding::TypeErasedHandler&& handler) noexcept;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_GENERIC_SKELETON_METHOD_H_
