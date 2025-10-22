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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_I_TYPE_ERASED_CALL_QUEUE_MOCK_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_I_TYPE_ERASED_CALL_QUEUE_MOCK_H

#include "score/mw/com/impl/bindings/lola/methods/i_type_erased_call_queue.h"

#include <gmock/gmock.h>

namespace score::mw::com::impl::lola
{

class TypeErasedCallQueueMock : public ITypeErasedCallQueue
{
  public:
    ~TypeErasedCallQueueMock() override = default;

    MOCK_METHOD(std::optional<score::cpp::span<std::byte>>, GetInArgsStorage, (size_t position), (const, override));
    MOCK_METHOD(std::optional<score::cpp::span<std::byte>>, GetResultStorage, (size_t position), (const, override));
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_I_TYPE_ERASED_CALL_QUEUE_MOCK_H
