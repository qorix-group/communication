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
#ifndef SCORE_MW_COM_IMPL_MOCKING_PROXY_FIELD_MOCK_H
#define SCORE_MW_COM_IMPL_MOCKING_PROXY_FIELD_MOCK_H

#include "score/mw/com/impl/event_receive_handler.h"
#include "score/mw/com/impl/plumbing/sample_ptr.h"
#include "score/mw/com/impl/subscription_state.h"

#include "score/result/result.h"

#include <score/callback.hpp>

#include <cstdint>

namespace score::mw::com::impl
{

class IProxyFieldBase
{
  public:
    IProxyFieldBase() = default;
    virtual ~IProxyFieldBase() = default;

    virtual ResultBlank Subscribe(const std::size_t) = 0;
    virtual void Unsubscribe() = 0;
    virtual SubscriptionState GetSubscriptionState() const = 0;
    virtual std::size_t GetFreeSampleCount() const = 0;
    virtual Result<std::size_t> GetNumNewSamplesAvailable() = 0;
    virtual ResultBlank SetReceiveHandler(EventReceiveHandler) = 0;
    virtual ResultBlank UnsetReceiveHandler() = 0;

  protected:
    IProxyFieldBase(const IProxyFieldBase&) = default;
    IProxyFieldBase(IProxyFieldBase&&) noexcept = default;
    IProxyFieldBase& operator=(IProxyFieldBase&&) & noexcept = default;
    IProxyFieldBase& operator=(const IProxyFieldBase&) & = default;
};

template <typename SampleType>
class IProxyField : public IProxyFieldBase
{
  public:
    IProxyField() = default;
    virtual ~IProxyField() = default;

    using Callback = score::cpp::callback<void(SamplePtr<SampleType>), 80U>;

    virtual Result<std::size_t> GetNewSamples(Callback&&, const std::size_t) = 0;

  protected:
    IProxyField(const IProxyField&) = default;
    IProxyField(IProxyField&&) noexcept = default;
    IProxyField& operator=(IProxyField&&) & noexcept = default;
    IProxyField& operator=(const IProxyField&) & = default;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_MOCKING_PROXY_FIELD_MOCK_H
