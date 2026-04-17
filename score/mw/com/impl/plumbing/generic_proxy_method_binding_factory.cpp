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
#include "score/mw/com/impl/plumbing/generic_proxy_method_binding_factory.h"

#include "score/mw/com/impl/bindings/lola/methods/type_erased_call_queue.h"
#include "score/mw/com/impl/bindings/lola/proxy.h"
#include "score/mw/com/impl/bindings/lola/proxy_method.h"
#include "score/mw/com/impl/configuration/binding_service_type_deployment_impl.h"
#include "score/mw/com/impl/configuration/lola_method_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/lola_service_type_deployment.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/proxy_base.h"
#include "score/mw/com/impl/service_element_type.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>
#include <score/overload.hpp>

#include <memory>
#include <string>

namespace score::mw::com::impl
{

namespace
{

LolaMethodInstanceDeployment::QueueSize GetQueueSize(const HandleType& parent_handle,
                                                      const std::string& method_name)
{
    const auto& lola_instance_deployment =
        GetServiceInstanceDeploymentBinding<LolaServiceInstanceDeployment>(
            parent_handle.GetServiceInstanceDeployment());

    const auto method_it = lola_instance_deployment.methods_.find(method_name);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(method_it != lola_instance_deployment.methods_.end(),
                                                "Method name not found in LolaServiceInstanceDeployment");
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(method_it->second.queue_size_.has_value(),
                                                "ProxyMethod cannot be created: queue_size not configured");
    return method_it->second.queue_size_.value();
}

}  // namespace

std::unique_ptr<ProxyMethodBinding> GenericProxyMethodBindingFactory::Create(HandleType parent_handle,
                                                                              ProxyBinding* parent_binding,
                                                                              const std::string_view method_name,
                                                                              MethodSizeInfo size_info) noexcept
{
    const auto method_name_str = std::string{method_name};
    const auto& type_deployment = parent_handle.GetServiceTypeDeployment();

    using ReturnType = std::unique_ptr<ProxyMethodBinding>;

    auto lola_handler = [&parent_handle, parent_binding, &method_name_str, &size_info](
                            const LolaServiceTypeDeployment& lola_type_deployment) -> ReturnType {
        auto* const lola_parent = dynamic_cast<lola::Proxy*>(parent_binding);
        if (lola_parent == nullptr)
        {
            score::mw::log::LogError("lola")
                << "GenericProxyMethodBindingFactory: parent binding is null or not a lola::Proxy";
            return nullptr;
        }

        const auto instance_id = parent_handle.GetInstanceId();
        const auto* const lola_instance_id = std::get_if<LolaServiceInstanceId>(&instance_id.binding_info_);
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(lola_instance_id != nullptr,
                                                    "ServiceInstanceId does not contain lola binding.");

        constexpr auto kElementType = ServiceElementType::METHOD;
        const auto lola_method_id = GetServiceElementId<kElementType>(lola_type_deployment, method_name_str);
        const lola::ElementFqId element_fq_id{
            lola_type_deployment.service_id_, lola_method_id, lola_instance_id->GetId(), kElementType};

        const auto queue_size = GetQueueSize(parent_handle, method_name_str);
        const lola::TypeErasedCallQueue::TypeErasedElementInfo type_erased_info{
            size_info.in_args, size_info.return_type, queue_size};

        return std::make_unique<lola::ProxyMethod>(*lola_parent, element_fq_id, type_erased_info);
    };

    auto visitor = score::cpp::overload(
        lola_handler,
        [](const score::cpp::blank&) noexcept -> ReturnType {
            return nullptr;
        },
        [](const SomeIpServiceInstanceDeployment&) noexcept -> ReturnType {
            return nullptr;
        });

    return std::visit(visitor, type_deployment.binding_info_);
}

}  // namespace score::mw::com::impl
