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
#ifndef SCORE_MW_COM_TEST_TYPES_H
#define SCORE_MW_COM_TEST_TYPES_H

#include "score/mw/com/types.h"

#include "score/mw/com/impl/handle_type.h"

#include <cstdint>

/// @brief This header contains all of the public functions and types that should be used by a user in order to mock
/// mw::com.
///
/// test_types does not contain any implementation, but instead simply exposes specific internals of mw::com.
namespace score::mw::com
{

using HandleType = impl::HandleType;

InstanceIdentifier MakeFakeInstanceIdentifier(const std::uint16_t unique_identifier);

HandleType MakeFakeHandle(const std::uint16_t unique_identifier);

void ResetInstanceIdentifierConfiguration();

}  // namespace score::mw::com

#endif  // SCORE_MW_COM_TEST_TYPES_H
