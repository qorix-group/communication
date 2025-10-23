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
#ifndef SCORE_MW_COM_MOCKING_RUNTIME_MOCK_H
#define SCORE_MW_COM_MOCKING_RUNTIME_MOCK_H

#include "score/mw/com/runtime_configuration.h"

#include "score/mw/com/types.h"

#include "score/memory/string_literal.h"
#include "score/result/result.h"

#include <score/span.hpp>

#include <cstdint>

namespace score::mw::com::runtime
{

class RuntimeMock
{
  public:
    RuntimeMock() = default;
    virtual ~RuntimeMock() = default;

    virtual score::Result<InstanceIdentifierContainer> ResolveInstanceIDs(const InstanceSpecifier model_name) = 0;
    virtual void InitializeRuntime(const std::int32_t argc, score::cpp::span<const score::StringLiteral> argv) = 0;
    virtual void InitializeRuntime(const runtime::RuntimeConfiguration& runtime_configuration) = 0;

  protected:
    RuntimeMock(const RuntimeMock&) = default;
    RuntimeMock(RuntimeMock&&) noexcept = default;
    RuntimeMock& operator=(RuntimeMock&&) & noexcept = default;
    RuntimeMock& operator=(const RuntimeMock&) & = default;
};

}  // namespace score::mw::com::runtime

#endif  // SCORE_MW_COM_MOCKING_RUNTIME_MOCK_H
