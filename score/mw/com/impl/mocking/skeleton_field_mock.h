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
#ifndef SCORE_MW_COM_IMPL_MOCKING_SKELETON_FIELD_MOCK_H
#define SCORE_MW_COM_IMPL_MOCKING_SKELETON_FIELD_MOCK_H

#include "score/mw/com/impl/plumbing/sample_allocatee_ptr.h"

#include "score/result/result.h"

#include <cstdint>

namespace score::mw::com::impl
{

template <typename SampleType>
class SkeletonFieldMock
{
  public:
    SkeletonFieldMock() = default;
    virtual ~SkeletonFieldMock() = default;

    virtual ResultBlank Update(const SampleType&) = 0;
    virtual ResultBlank Update(SampleAllocateePtr<SampleType>) = 0;
    virtual Result<SampleAllocateePtr<SampleType>> Allocate() = 0;

  protected:
    SkeletonFieldMock(const SkeletonFieldMock&) = default;
    SkeletonFieldMock(SkeletonFieldMock&&) noexcept = default;
    SkeletonFieldMock& operator=(SkeletonFieldMock&&) & noexcept = default;
    SkeletonFieldMock& operator=(const SkeletonFieldMock&) & = default;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_MOCKING_SKELETON_FIELD_MOCK_H
