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
#ifndef SCORE_MW_COM_IMPL_MOCKING_SKELETON_MOCK_H
#define SCORE_MW_COM_IMPL_MOCKING_SKELETON_MOCK_H

#include "score/result/result.h"

#include <cstdint>

namespace score::mw::com::impl
{

class ISkeleton
{
  public:
    ISkeleton() = default;
    virtual ~ISkeleton() = default;

    virtual ResultBlank OfferService() = 0;
    virtual void StopOfferService() = 0;

  protected:
    ISkeleton(const ISkeleton&) = default;
    ISkeleton(ISkeleton&&) noexcept = default;
    ISkeleton& operator=(ISkeleton&&) & noexcept = default;
    ISkeleton& operator=(const ISkeleton&) & = default;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_MOCKING_SKELETON_MOCK_H
