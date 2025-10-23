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
#ifndef SCORE_MW_COM_IMPL_TRAITS_H
#define SCORE_MW_COM_IMPL_TRAITS_H

#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/handle_type.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/plumbing/proxy_binding_factory.h"
#include "score/mw/com/impl/plumbing/skeleton_binding_factory.h"
#include "score/mw/com/impl/proxy_base.h"
#include "score/mw/com/impl/proxy_event.h"
#include "score/mw/com/impl/proxy_field.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/com/impl/skeleton_event.h"
#include "score/mw/com/impl/skeleton_field.h"

#include "score/result/result.h"
#include "score/mw/log/logging.h"

#include <score/utility.hpp>

#include <exception>
#include <optional>
#include <queue>
#include <unordered_map>
#include <utility>
#include <vector>

namespace score::mw::com::impl
{
namespace detail
{

template <typename Queue>
auto PopFront(Queue& queue) -> typename Queue::value_type
{
    auto front_element = std::move(queue.front());
    queue.pop();
    return front_element;
}

template <typename Key, typename MapOfQueues>
auto ExtractCreationResultFrom(const Key& key, MapOfQueues& map_of_queues)
{
    auto queue_it = map_of_queues.find(key);
    if (queue_it == map_of_queues.cend())
    {
        score::mw::log::LogFatal("lola") << "Could not find key in injected creation results!";
        std::terminate();
    }

    auto& creation_results_queue = queue_it->second;
    if (creation_results_queue.empty())
    {
        score::mw::log::LogFatal("lola") << "No inject result exists in the provided vector!";
        std::terminate();
    }

    return PopFront(creation_results_queue);
}

}  // namespace detail

template <typename T>
class SkeletonWrapperClassTestView;

template <typename T>
class ProxyWrapperClassTestView;

/// The main idea of these traits are to ease the interface creation for a user. It reduces the necessary generated code
/// to a bare minimum.
///
/// Thanks to these traits a user can construct an interface as follows:
///
/// template <typename Trait>
/// class TheInterface : public Trait::Base
/// {
///   public:
///     using Trait::Base::Base;
///
///     typename Trait::template Event<DataType1> struct_event_1_{*this, event_name_0};
///     typename Trait::template Event<DataType2> struct_event_2_{*this, event_name_1};
///
///     typename Trait::template Field<DataType1> struct_field_1_{*this, field_name_0};
///     typename Trait::template Field<DataType2> struct_field_2_{*this, field_name_1};
///
/// };
///
/// It is then possible to interpret this interface as proxy or skeleton as `using TheProxy = AsProxy<TheInterface>`.
/// It shall be noted, that the data types used, need to by PolymorphicOffsetPtrAllocator aware.

/// \brief Encapsulates all necessary attributes for a proxy, which will then be used by an interface that uses traits
class ProxyTrait
{
  public:
    using Base = ProxyBase;

    template <typename SampleType>
    using Event = ProxyEvent<SampleType>;

    template <typename SampleType>
    using Field = ProxyField<SampleType>;
};

/// \brief Encapsulates all necessary attributes for a skeleton, which will then be used by an interface that uses
/// traits
class SkeletonTrait
{
  public:
    using Base = SkeletonBase;

    template <typename SampleType>
    using Event = SkeletonEvent<SampleType>;

    template <typename SampleType>
    using Field = SkeletonField<SampleType>;
};

template <template <class> class Interface, class Trait>
// Passed template parameter \p Interface could be a struct that violates Autosar rule A11-0-2: "not be a base of
// another struct or class". The fix may involve extra effort to replace particular structs with class types every used
// entrance and/or special check whether passed template parameter is a class.
// NOLINTNEXTLINE(score-struct-usage-compliance): Tolerated.
class SkeletonWrapperClass : public Interface<Trait>
{
    friend class SkeletonWrapperClassTestView<SkeletonWrapperClass>;

  public:
    static Result<SkeletonWrapperClass> Create(
        const InstanceSpecifier& specifier,
        MethodCallProcessingMode mode = MethodCallProcessingMode::kEvent) noexcept
    {
        if (instance_specifier_creation_results_.has_value())
        {
            return detail::ExtractCreationResultFrom(specifier, instance_specifier_creation_results_.value());
        }

        const auto instance_identifier_result = GetInstanceIdentifier(specifier);
        if (!instance_identifier_result.has_value())
        {
            score::mw::log::LogFatal("lola") << "Failed to resolve instance identifier from instance specifier";
            std::terminate();
        }
        return Create(instance_identifier_result.value(), mode);
    }

    static Result<SkeletonWrapperClass> Create(
        const InstanceIdentifier& instance_identifier,
        MethodCallProcessingMode mode = MethodCallProcessingMode::kEvent) noexcept
    {
        if (instance_specifier_creation_results_.has_value())
        {
            return detail::ExtractCreationResultFrom(instance_identifier,
                                                     instance_identifier_creation_results_.value());
        }

        auto skeleton_binding = SkeletonBindingFactory::Create(instance_identifier);
        SkeletonWrapperClass skeleton_wrapper(instance_identifier, std::move(skeleton_binding), mode);
        if (!skeleton_wrapper.AreBindingsValid())
        {
            ::score::mw::log::LogError("lola") << "Could not create SkeletonWrapperClass as Skeleton binding or service "
                                                "element bindings could not be created.";
            return MakeUnexpected(ComErrc::kBindingFailure);
        }

        return skeleton_wrapper;
    }

  private:
    explicit SkeletonWrapperClass(const InstanceIdentifier& instance_id,
                                  std::unique_ptr<SkeletonBinding> skeleton_binding,
                                  MethodCallProcessingMode mode = MethodCallProcessingMode::kEvent)
        : Interface<Trait>{std::move(skeleton_binding), instance_id, mode}
    {
    }

    static void InjectCreationResults(std::unordered_map<InstanceSpecifier, std::queue<Result<SkeletonWrapperClass>>>
                                          instance_specifier_creation_results,
                                      std::unordered_map<InstanceIdentifier, std::queue<Result<SkeletonWrapperClass>>>
                                          instance_identifier_creation_results)
    {
        score::cpp::ignore = instance_specifier_creation_results_.emplace(std::move(instance_specifier_creation_results));
        score::cpp::ignore = instance_identifier_creation_results_.emplace(std::move(instance_identifier_creation_results));
    }

    static void ClearCreationResults()
    {
        instance_specifier_creation_results_.reset();
        instance_identifier_creation_results_.reset();
    }

    static std::optional<std::unordered_map<InstanceSpecifier, std::queue<Result<SkeletonWrapperClass>>>>
        instance_specifier_creation_results_;
    static std::optional<std::unordered_map<InstanceIdentifier, std::queue<Result<SkeletonWrapperClass>>>>
        instance_identifier_creation_results_;
};
template <template <class> class Interface, class Trait>
std::optional<std::unordered_map<InstanceSpecifier, std::queue<Result<SkeletonWrapperClass<Interface, Trait>>>>>
    SkeletonWrapperClass<Interface, Trait>::instance_specifier_creation_results_{};
template <template <class> class Interface, class Trait>
std::optional<std::unordered_map<InstanceIdentifier, std::queue<Result<SkeletonWrapperClass<Interface, Trait>>>>>
    SkeletonWrapperClass<Interface, Trait>::instance_identifier_creation_results_{};

template <template <class> class Interface, class Trait>
class ProxyWrapperClass : public Interface<Trait>
{
    friend class ProxyWrapperClassTestView<ProxyWrapperClass>;

  public:
    /// \brief Execption-less ProxyWrapperClass constructor
    static Result<ProxyWrapperClass> Create(HandleType instance_handle) noexcept
    {
        if (creation_results_.has_value())
        {
            return detail::ExtractCreationResultFrom(instance_handle, creation_results_.value());
        }

        auto proxy_binding = ProxyBindingFactory::Create(instance_handle);
        ProxyWrapperClass proxy_wrapper(instance_handle, std::move(proxy_binding));
        if (!proxy_wrapper.AreBindingsValid())
        {
            ::score::mw::log::LogError("lola")
                << "Could not create ProxyWrapperClass as Proxy binding or service element "
                   "bindings could not be created.";
            return MakeUnexpected(ComErrc::kBindingFailure);
        }
        return proxy_wrapper;
    }

  private:
    /// \brief Constructs ProxyWrapperClass
    explicit ProxyWrapperClass(HandleType instance_handle, std::unique_ptr<ProxyBinding> proxy_binding)
        : Interface<Trait>{std::move(proxy_binding), std::move(instance_handle)}
    {
    }

    ProxyWrapperClass() : Interface<Trait>{} {}

    static void InjectCreationResults(
        std::unordered_map<HandleType, std::queue<Result<ProxyWrapperClass>>> creation_results)
    {
        score::cpp::ignore = creation_results_.emplace(std::move(creation_results));
    }

    static void ClearCreationResults()
    {
        creation_results_.reset();
    }

    static std::optional<std::unordered_map<HandleType, std::queue<Result<ProxyWrapperClass>>>> creation_results_;
};
template <template <class> class Interface, class Trait>
std::optional<std::unordered_map<HandleType, std::queue<Result<ProxyWrapperClass<Interface, Trait>>>>>
    ProxyWrapperClass<Interface, Trait>::creation_results_{};

/// \brief Interpret an interface that follows our traits as proxy (see description above)
template <template <class> class T>
using AsProxy = ProxyWrapperClass<T, ProxyTrait>;

/// \brief Interpret an interface that follows our traits as skeleton (see description above)
template <template <class> class T>
using AsSkeleton = SkeletonWrapperClass<T, SkeletonTrait>;

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_TRAITS_H
