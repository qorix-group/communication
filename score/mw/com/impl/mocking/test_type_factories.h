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
#ifndef SCORE_MW_COM_IMPL_MOCKING_TEST_TYPE_FACTORIES_H
#define SCORE_MW_COM_IMPL_MOCKING_TEST_TYPE_FACTORIES_H

#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_identifier.h"

#include <cstdint>
#include <list>

/// @brief test_type_factories contains all of the factory functions that are required for creating fake mw::com::impl
/// internals which are required for mocking.
///
/// These types and functions should not be accessed directly by applications, but rather they should use
/// mw/com/test_types.h
namespace score::mw::com::impl
{

InstanceIdentifier MakeFakeInstanceIdentifier(const std::uint16_t unique_identifier);

HandleType MakeFakeHandle(const std::uint16_t unique_identifier);

void ResetInstanceIdentifierConfiguration();

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_MOCKING_TEST_TYPE_FACTORIES_H
