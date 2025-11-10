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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_SKELETON_METHOD_H
#define SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_SKELETON_METHOD_H

#include "score/mw/com/impl/skeleton_method_binding.h"

#include <gmock/gmock.h>

namespace score::mw::com::impl::mock_binding
{

class SkeletonMethod : public SkeletonMethodBinding
{
  public:
    using SkeletonMethodBinding::SkeletonMethodBinding;
    ~SkeletonMethod() override = default;
    MOCK_METHOD(ResultBlank, Register, (TypeErasedCallback&&), (override));
};

}  // namespace score::mw::com::impl::mock_binding

#endif  // SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_SKELETON_METHOD_H
