/*******************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 *******************************************************************************/

#ifndef SCORE_MW_COM_TEST_FIELDS_SET_AND_NOTIFIER_TEST_CONSTANTS_H
#define SCORE_MW_COM_TEST_FIELDS_SET_AND_NOTIFIER_TEST_CONSTANTS_H

#include <cstddef>
#include <cstdint>

namespace score::mw::com::test
{

constexpr const char* const kInstanceSpecifierString = "/score/mw/com/test/fields/set_and_get_and_notifier_instance";

constexpr std::int32_t kInitialValue = 18;
constexpr std::int32_t kSetRequestValue = 1234;
constexpr std::int32_t kUpdatedValue = 19;

constexpr std::size_t kTotalNumValuesToSend = 3U;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_FIELDS_SET_AND_NOTIFIER_TEST_CONSTANTS_H
