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
#include "score/mw/com/impl/skeleton_method_base.h"

namespace score::mw::com::impl
{
void SkeletonMethodBase::UpdateSkeletonReference(SkeletonBase& skeleton_base) noexcept
{
    skeleton_base_ = skeleton_base;
}
}  // namespace score::mw::com::impl
