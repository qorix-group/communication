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
#ifndef SCORE_MW_COM_IMPL_MOCKING_SKELETON_MOCK_IMPL_H
#define SCORE_MW_COM_IMPL_MOCKING_SKELETON_MOCK_IMPL_H

#include "score/mw/com/impl/mocking/i_skeleton.h"

#include <gmock/gmock.h>

namespace score::mw::com::impl
{

class SkeletonMockImpl : public ISkeleton
{
  public:
    MOCK_METHOD(ResultBlank, OfferService, (), (override));
    MOCK_METHOD(void, StopOfferService, (), (override));
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_MOCKING_SKELETON_MOCK_IMPL_H
