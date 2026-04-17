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
#include "score/mw/com/impl/generic_skeleton_method.h"

#include "score/mw/com/impl/skeleton_base.h"

namespace score::mw::com::impl
{

GenericSkeletonMethod::GenericSkeletonMethod(SkeletonBase& skeleton_base,
                                             const std::string_view method_name,
                                             std::unique_ptr<SkeletonMethodBinding> binding)
    : SkeletonMethodBase(skeleton_base, method_name, std::move(binding))
{
    SkeletonBaseView{skeleton_base}.RegisterMethod(method_name_, *this);
}

GenericSkeletonMethod::GenericSkeletonMethod(GenericSkeletonMethod&& other) noexcept
    : SkeletonMethodBase(std::move(other))
{
    SkeletonBaseView skeleton_base_view{skeleton_base_.get()};
    skeleton_base_view.UpdateMethod(method_name_, *this);
}

GenericSkeletonMethod& GenericSkeletonMethod::operator=(GenericSkeletonMethod&& other) & noexcept
{
    if (this != &other)
    {
        SkeletonMethodBase::operator=(std::move(other));
        SkeletonBaseView skeleton_base_view{skeleton_base_.get()};
        skeleton_base_view.UpdateMethod(method_name_, *this);
    }
    return *this;
}

ResultBlank GenericSkeletonMethod::RegisterHandler(SkeletonMethodBinding::TypeErasedHandler&& handler) noexcept
{
    return binding_->RegisterHandler(std::move(handler));
}

}  // namespace score::mw::com::impl
