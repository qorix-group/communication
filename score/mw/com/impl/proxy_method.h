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
#ifndef SCORE_MW_COM_IMPL_PROXY_METHOD_H
#define SCORE_MW_COM_IMPL_PROXY_METHOD_H

#include "score/containers/dynamic_array.h"
#include "score/result/result.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/method_signature_element_ptr.h"
#include "score/mw/com/impl/proxy_base.h"
#include "score/mw/com/impl/proxy_method_binding.h"
#include "score/mw/com/impl/util/type_erased_storage.h"

#include <algorithm>
#include <optional>

namespace score::mw::com::impl
{

namespace detail
{
template <typename... Args>
constexpr std::optional<TypeErasedDataTypeInfo> InitTypeErasedInArgs()
{
    if constexpr (sizeof...(Args) > 0)
    {
        return CreateTypeErasedDataTypeInfoFromTypes<Args...>();
    }
    else
    {
        return std::nullopt;
    }
}

template <typename R>
constexpr std::optional<TypeErasedDataTypeInfo> InitTypeErasedReturnType()
{
    if constexpr (!std::is_void<R>::value)
    {
        return CreateTypeErasedDataTypeInfoFromTypes<R>();
    }
    else
    {
        return std::nullopt;
    }
}

template <typename... ArgTypes>
using enable_if_in_args_exist = typename std::enable_if_t<(sizeof...(ArgTypes) > 0)>;

template <typename R>
using enable_if_non_void_return = typename std::enable_if_t<!std::is_void<R>::value>;

template <typename R>
using enable_if_void_return = typename std::enable_if_t<std::is_void<R>::value>;

}  // namespace detail

/// \brief Primary template of ProxyMethod. This is the "catch all" case for ProxyMethod class template instantiation.
/// \details This template only exists to provide a static_assert, that gives a meaningful error message, when
/// ProxyMethod is instantiated with an unsupported signature type.
template <typename Signature>
class ProxyMethod
{
    static_assert(
        sizeof(Signature) == 0,
        "ProxyMethod can only be instantiated with a function signature, e.g., ProxyMethod<void(int, double)>. "
        "You tried to use an unsupported signature type.");
};

/// \brief Partial specialization of ProxyMethod for function signatures with no arguments.
/// \details This specialization exists to handle the special case of zero-argument methods, as some internal logic
/// needs to be adapted for this case.
template <typename ReturnType>
class ProxyMethod<ReturnType()>
{
    template <typename R, typename... A>
    // Design decision: This friend class provides a view on the internals of ProxyMethod.
    // This enables us to hide unnecessary internals from the end-user.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class ProxyMethodView;

  public:
    ProxyMethod(ProxyBase& /* proxy_base */,
                std::unique_ptr<ProxyMethodBinding> proxy_method_binding,
                std::string_view method_name) noexcept
        : method_name_{method_name},
          is_return_type_ptr_active_{kCallQueueSize, false},
          binding_{std::move(proxy_method_binding)}
    {
    }

    /// \brief A ProxyMethod shall not be copyable. (Exactly like impl::ProxyBase and impl:ProxyEventBase)
    ProxyMethod(const ProxyMethod&) = delete;
    ProxyMethod& operator=(const ProxyMethod&) = delete;

    /// \brief A ProxyMethod shall be moveable. (Exactly like impl::ProxyBase and impl:ProxyEventBase)
    ProxyMethod(ProxyMethod&&) = default;
    ProxyMethod& operator=(ProxyMethod&&) = default;

    /// \brief This is the copying call-operator of ProxyMethod for a non-void ReturnType.
    /// \details This call-operator variant takes the argument values as references. It then internally calls Allocate()
    /// to get the needed storage for the arguments and return value and copies the arguments into the allocated
    /// storage.
    template <typename Dummy = detail::enable_if_non_void_return<ReturnType>>
    score::Result<MethodReturnTypePtr<ReturnType>> operator()();

    /// \brief This is the copying call-operator of ProxyMethod for a void ReturnType.
    /// \details This call-operator variant takes the argument values as references.
    template <typename Dummy = detail::enable_if_void_return<ReturnType>>
    score::ResultBlank operator()();

  private:
    score::Result<std::size_t> AllocateNextAvailableQueueSlot();

    /// \brief Empty optional as in this class template specialization we do not have in-arguments.
    /// \details We still keep this member for interface consistency with the general ProxyMethod template
    /// specialization. The access via ProxyMethodView remains the same.
    static constexpr std::optional<TypeErasedDataTypeInfo> type_erased_in_args_{};

    /// \brief Compile-time initialized TypeErasedDataTypeInfo for the return type of this ProxyMethod.
    /// \details This is the only information about the return type of this Proxy Method, which is available at
    /// runtime. It is handed down to the binding layer, which then does the type agnostic transport.
    /// \remark If the return type is void, this is std::nullopt.
    static constexpr std::optional<TypeErasedDataTypeInfo> type_erased_return_type_ =
        detail::InitTypeErasedReturnType<ReturnType>();

    /// \brief Size of the call-queue is currently fixed to 1! As soon as we are going to support larger call-queues,
    /// the call-queue-size shall be taken from configuration and handed over to ProxyMethod ctor.
    static constexpr containers::DynamicArray<int>::size_type kCallQueueSize = 1U;

    std::string_view method_name_;

    /// \brief Dynamic array containing queue-slot active flags: one entry per call-queue position.
    /// \details This array contains bool flags, which indicate, if the return value pointer
    /// returned from a call-operator is active (true), i.e. still in-use by the user or not (false).
    ///
    /// This array is used in these two cases slightly differently:
    /// In the case, that the return type is non-void, the flag indicates, that the return value pointer handed out via
    /// the call-operator for the given call-queue position is still active (true) or not (false).
    /// In the case of a void return type, the flag indicates, that a call at the given call-queue position is still in
    /// progress (true) or not (false). In any case the related queue slot is considered "in-use". As long as we only
    /// support synchronous method calls, the latter case (void return type) doesn't use this array, because "queueing"
    /// (when we had a queue-size > 1) in a synchronous call setup only works for the allocation of in-args (Allocate()
    /// calls), not for the call-operator itself. But since this template specialization has no in-args/Allocate(),
    /// there is also no "queuing" for in-arg allocations. Therefore, in the void-return case, this array will only be
    /// used in a future async call-operator: There it will set the queue-position related flag to "true" at the start
    /// of the async call and back to false, when the asynchronous call concludes.
    containers::DynamicArray<bool> is_return_type_ptr_active_;

    std::unique_ptr<ProxyMethodBinding> binding_;
};

template <typename ReturnType, typename... ArgTypes>
class ProxyMethod<ReturnType(ArgTypes...)> final
{
    template <typename R, typename... A>
    // Design decision: This friend class provides a view on the internals of ProxyMethod.
    // This enables us to hide unnecessary internals from the end-user.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class ProxyMethodView;

  public:
    ProxyMethod(ProxyBase& /* proxy_base */,
                std::unique_ptr<ProxyMethodBinding> proxy_method_binding,
                std::string_view method_name) noexcept
        : method_name_{method_name},
          is_in_arg_ptr_active_{kCallQueueSize},
          is_return_type_ptr_active_{kCallQueueSize, false},
          binding_{std::move(proxy_method_binding)}
    {
    }

    /// \brief A ProxyMethod shall not be copyable. (Exactly like impl::ProxyBase and impl:ProxyEventBase)
    ProxyMethod(const ProxyMethod&) = delete;
    ProxyMethod& operator=(const ProxyMethod&) = delete;

    /// \brief A ProxyMethod shall be moveable. (Exactly like impl::ProxyBase and impl:ProxyEventBase)
    ProxyMethod(ProxyMethod&&) = default;
    ProxyMethod& operator=(ProxyMethod&&) = default;

    /// \brief Allocates the necessary storage for the argument values and the return value of a method call.
    /// \return On success, a tuple of MethodInArgPtr for each argument type is returned. On failure, an error code is
    /// returned.
    template <typename Dummy = detail::enable_if_in_args_exist<ArgTypes...>>
    score::Result<std::tuple<impl::MethodInArgPtr<ArgTypes>...>> Allocate();

    /// \brief This is the copying call-operator of ProxyMethod for a non-void ReturnType.
    /// \details This call-operator variant takes the argument values as references. It then internally calls Allocate()
    /// to get the needed storage for the arguments and return value and copies the arguments into the allocated
    /// storage.
    template <typename Dummy = detail::enable_if_non_void_return<ReturnType>>
    score::Result<MethodReturnTypePtr<ReturnType>> operator()(const ArgTypes&... args);

    /// \brief This is the zero-copy call-operator of ProxyMethod for a non-void ReturnType.
    /// \details This call-operator variant takes the argument values as MethodInArgPtr, i.e. as pointers to the
    /// argument values, which have been allocated before via Allocate() call.
    template <typename Dummy = detail::enable_if_non_void_return<ReturnType>>
    score::Result<MethodReturnTypePtr<ReturnType>> operator()(MethodInArgPtr<ArgTypes>... args);

    /// \brief This is the copying call-operator of ProxyMethod for a void ReturnType.
    /// \details This call-operator variant takes the argument values as references.
    template <typename Dummy = detail::enable_if_void_return<ReturnType>>
    score::ResultBlank operator()(const ArgTypes&... args);

    /// \brief This is the zero-copy call-operator of ProxyMethod for a void ReturnType. It only exists if there is at
    /// least one argument.
    /// \details This call-operator variant takes the argument values as MethodInArgPtr, i.e. as pointers to the
    /// argument values, which have been allocated before via Allocate() call.
    template <typename Dummy = detail::enable_if_void_return<ReturnType>>
    score::ResultBlank operator()(MethodInArgPtr<ArgTypes>... args);

  private:
    score::Result<std::size_t> AllocateNextAvailableQueueSlot();

    /// \brief Checks, that all MethodInArgPtr arguments have the same queue_position_ and returns this common value.
    /// \details Will assert/terminate, if the queue_position_ values differ.
    /// \tparam MethodInArgPtrs Variadic template parameter pack for MethodInArgPtr types.
    /// \param args MethodInArgPtr arguments to check.
    /// \return The common queue_position_ value.
    ///
    /// \remark We are not checking, whether the user handed over MethodInArgPtr from different ProxyMethod
    /// instances. This would require more type information at runtime, which we don't have. Therefore, we just check,
    /// that all queue_position_ values are the same.
    template <typename... MethodInArgPtrs>
    static std::size_t GetCommonQueuePosition(const MethodInArgPtrs&... args);

    /// \brief Creates a tuple of MethodInArgPtr for the given argument types from the given tuple of raw pointers.
    /// \tparam I Compile-time index sequence for the argument types.
    /// \param ptrs Tuple of raw pointers to the argument values.
    /// \param queue_index Queue index to be set in each MethodInArgPtr.
    /// \return Tuple of MethodInArgPtr for each argument type.
    template <std::size_t... I>
    std::tuple<impl::MethodInArgPtr<ArgTypes>...> CreateMethodInArgPtrTuple(
        const std::tuple<ArgTypes*...>& ptrs,
        int queue_index,
        std::index_sequence<I...> = std::make_index_sequence<sizeof...(ArgTypes)>());

    /// \brief Compile-time initialized TypeErasedDataTypeInfo for the argument types of this ProxyMethod.
    /// \details This is the only information about the argument types of this Proxy Method, which is available at
    /// runtime. It is handed down to the binding layer, which then does the type agnostic transport.
    /// \remark If there are no arguments, this is std::nullopt.
    static constexpr std::optional<TypeErasedDataTypeInfo> type_erased_in_args_ =
        detail::InitTypeErasedInArgs<ArgTypes...>();

    /// \brief Compile-time initialized TypeErasedDataTypeInfo for the return type of this ProxyMethod.
    /// \details This is the only information about the return type of this Proxy Method, which is available at
    /// runtime. It is handed down to the binding layer, which then does the type agnostic transport.
    /// \remark If the return type is void, this is std::nullopt.
    static constexpr std::optional<TypeErasedDataTypeInfo> type_erased_return_type_ =
        detail::InitTypeErasedReturnType<ReturnType>();

    /// \brief Size of the call-queue is currently fixed to 1! As soon as we are going to support larger call-queues,
    /// the call-queue-size shall be taken from configuration and handed over to ProxyMethod ctor.
    static constexpr containers::DynamicArray<int>::size_type kCallQueueSize = 1U;

    std::string_view method_name_;

    /// \brief Outer dynamic array: one entry per call-queue position, inner array: one entry per argument.
    /// \details This array of arrays contains bool flags, which indicate, if the corresponding argument pointer
    /// passed to the zero-copy call-operator is active (true) or not (false).
    /// E.g. is_in_arg_ptr_active_[0][2] == true means, that for the call-queue position 0, the 3rd argument
    /// pointer passed to the zero-copy call-operator is active.
    containers::DynamicArray<std::array<bool, sizeof...(ArgTypes)>> is_in_arg_ptr_active_;

    /// \brief Dynamic array containing queue-slot active flags: one entry per call-queue position.
    /// \details This array contains bool flags, which indicate, if the return value pointer
    /// returned from a call-operator is active (true), i.e. still in-use by the user or not (false).
    /// For further details see the documentation of is_return_type_ptr_active_ in the
    /// ProxyMethod<ReturnType()> specialization.
    containers::DynamicArray<bool> is_return_type_ptr_active_;

    std::unique_ptr<ProxyMethodBinding> binding_;
};

/// ---------------- Implementation of ProxyMethod class template specialization with empty arguments ----------------

template <typename ReturnType>
template <typename Dummy = detail::enable_if_non_void_return<ReturnType>>
score::Result<MethodReturnTypePtr<ReturnType>> ProxyMethod<ReturnType()>::operator()()
{
    auto queue_position_result = AllocateNextAvailableQueueSlot();
    if (!queue_position_result.has_value())
    {
        return Unexpected(queue_position_result.error());
    }

    const auto queue_position = queue_position_result.value();
    auto allocated_return_type_storage = binding_->AllocateReturnType(queue_position);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(allocated_return_type_storage.has_value(),
                           "ProxyMethod::operator(): AllocateReturnType failed unexpectedly.");
    // \todo (Ticket-221600): Clarify stop_token usage
    auto call_result = binding_->DoCall(queue_position, score::cpp::stop_token{});
    if (!call_result.has_value())
    {
        return Unexpected(call_result.error());
    }

    return MethodReturnTypePtr<ReturnType>{
        *(reinterpret_cast<ReturnType*>(allocated_return_type_storage.value().data())),
        is_return_type_ptr_active_[queue_position],
        queue_position};
}

template <typename ReturnType>
template <typename Dummy = detail::enable_if_void_return<ReturnType>>
score::ResultBlank ProxyMethod<ReturnType()>::operator()()
{
    auto queue_position = AllocateNextAvailableQueueSlot();
    if (!queue_position.has_value())
    {
        return Unexpected(queue_position.error());
    }
    // \todo (Ticket-221600): Clarify stop_token usage
    auto call_result = binding_->DoCall(queue_position.value(), score::cpp::stop_token{});
    if (!call_result.has_value())
    {
        return Unexpected(call_result.error());
    }

    return {};
}

template <typename ReturnType>
score::Result<std::size_t> ProxyMethod<ReturnType()>::AllocateNextAvailableQueueSlot()
{
    for (std::size_t i = 0U; i < is_return_type_ptr_active_.size(); ++i)
    {
        if (!is_return_type_ptr_active_[i])
        {
            return i;
        }
    }
    return score::MakeUnexpected(ComErrc::kCallQueueFull);
}

/// ---------------- Implementation of ProxyMethod class template with arguments ----------------

template <typename ReturnType, typename... ArgTypes>
template <typename Dummy = detail::enable_if_in_args_exist<ArgTypes...>>
score::Result<std::tuple<impl::MethodInArgPtr<ArgTypes>...>> ProxyMethod<ReturnType(ArgTypes...)>::Allocate()
{
    auto available_queue_slot = AllocateNextAvailableQueueSlot();
    if (!available_queue_slot.has_value())
    {
        return Unexpected(available_queue_slot.error());
    }
    const int queue_index = available_queue_slot.value();
    auto allocated_in_args_storage = binding_->AllocateInArgs(queue_index);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(allocated_in_args_storage.has_value(),
                           "ProxyMethod::Allocate: AllocateInArgs failed unexpectedly.");
    MemoryBufferAccessor in_args_buffer_accessor{(allocated_in_args_storage.value().data()),
                                                 allocated_in_args_storage.value().size()};
    const auto deserialized_arg_pointers = Deserialize<ArgTypes...>(in_args_buffer_accessor);
    auto method_in_arg_ptr_tuple = CreateMethodInArgPtrTuple(
        deserialized_arg_pointers, queue_index, std::make_index_sequence<sizeof...(ArgTypes)>());

    return score::Result<std::tuple<score::mw::com::impl::MethodInArgPtr<ArgTypes>...>>(std::move(method_in_arg_ptr_tuple));
}

template <typename ReturnType, typename... ArgTypes>
template <typename Dummy = detail::enable_if_non_void_return<ReturnType>>
score::Result<MethodReturnTypePtr<ReturnType>> ProxyMethod<ReturnType(ArgTypes...)>::operator()(const ArgTypes&... args)
{
    auto allocate_result = Allocate();
    if (!allocate_result.has_value())
    {
        return Unexpected(allocate_result.error());
    }
    auto& in_arg_ptr_tuple = allocate_result.value();

    // now copy the argument values into the allocated storage and call the other operator() taking MethodInArgPtr
    return std::apply(
        [this, &args...](auto&&... in_args_ptrs) {
            ((*(in_args_ptrs.get()) = args), ...);
            // attention: the explicit "this->" here was needed to make gcc8 happy! Without it, it crashed.
            return this->operator()(std::move(in_args_ptrs)...);
        },
        in_arg_ptr_tuple);
}

template <typename ReturnType, typename... ArgTypes>
template <typename Dummy = detail::enable_if_non_void_return<ReturnType>>
score::Result<MethodReturnTypePtr<ReturnType>> ProxyMethod<ReturnType(ArgTypes...)>::operator()(
    MethodInArgPtr<ArgTypes>... args)
{
    auto queue_position = GetCommonQueuePosition(args...);
    auto allocated_return_type_storage = binding_->AllocateReturnType(queue_position);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(allocated_return_type_storage.has_value(),
                           "ProxyMethod::operator(): AllocateReturnType failed unexpectedly.");
    // \todo (Ticket-221600): Clarify stop_token usage
    auto call_result = binding_->DoCall(queue_position, score::cpp::stop_token{});
    if (!call_result.has_value())
    {
        return Unexpected(call_result.error());
    }

    return MethodReturnTypePtr<ReturnType>{
        *(reinterpret_cast<ReturnType*>(allocated_return_type_storage.value().data())),
        is_return_type_ptr_active_[queue_position],
        queue_position};
}

template <typename ReturnType, typename... ArgTypes>
template <typename Dummy = detail::enable_if_void_return<ReturnType>>
score::ResultBlank ProxyMethod<ReturnType(ArgTypes...)>::operator()(const ArgTypes&... args)
{
    auto allocate_result = Allocate();
    if (!allocate_result.has_value())
    {
        return Unexpected(allocate_result.error());
    }
    auto& in_arg_ptr_tuple = allocate_result.value();

    // now copy the argument values into the allocated storage and call the other operator() taking MethodInArgPtr
    return std::apply(
        [&](auto&&... in_args_ptrs) {
            ((*(in_args_ptrs.get()) = args), ...);
            // attention: the explicit "this->" here was needed to make gcc8 happy! Without it, it crashed.
            return this->operator()(std::move(in_args_ptrs)...);
        },
        in_arg_ptr_tuple);
}

template <typename ReturnType, typename... ArgTypes>
template <typename Dummy = detail::enable_if_void_return<ReturnType>>
score::ResultBlank ProxyMethod<ReturnType(ArgTypes...)>::operator()(MethodInArgPtr<ArgTypes>... args)
{
    auto queue_position = GetCommonQueuePosition(args...);
    // \todo (Ticket-221600): Clarify stop_token usage
    auto call_result = binding_->DoCall(queue_position, score::cpp::stop_token{});
    if (!call_result.has_value())
    {
        return Unexpected(call_result.error());
    }
    return {};
}

template <typename ReturnType, typename... ArgTypes>
score::Result<std::size_t> ProxyMethod<ReturnType(ArgTypes...)>::AllocateNextAvailableQueueSlot()
{
    // Find an index, where in is_in_arg_ptr_active_ and is_return_type_ptr_active_ is inactive
    for (std::size_t i = 0U; i < is_in_arg_ptr_active_.size(); ++i)
    {
        bool all_inactive =
            std::none_of(is_in_arg_ptr_active_[i].begin(), is_in_arg_ptr_active_[i].end(), [](bool active) {
                return active;
            });
        if (all_inactive && (!is_return_type_ptr_active_[i]))
        {
            // Found an available slot
            return i;
        }
    }
    return score::MakeUnexpected(ComErrc::kCallQueueFull);
}

template <typename ReturnType, typename... ArgTypes>
template <typename... MethodInArgPtrs>
std::size_t ProxyMethod<ReturnType(ArgTypes...)>::GetCommonQueuePosition(const MethodInArgPtrs&... args)
{
    std::optional<std::size_t> expected_queue_position{};
    (([&expected_queue_position](const auto& method_in_arg_ptr) {
         if (!expected_queue_position.has_value())
         {
             expected_queue_position = method_in_arg_ptr.GetQueuePosition();
         }
         else
         {
             if (method_in_arg_ptr.GetQueuePosition() != expected_queue_position.value())
             {
                 SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(false, "All MethodInArgPtr arguments must have the same queue_position_");
             }
         }
     })(args),
     ...);
    return expected_queue_position.value();
}

template <typename ReturnType, typename... ArgTypes>
template <std::size_t... I>
std::tuple<impl::MethodInArgPtr<ArgTypes>...> ProxyMethod<ReturnType(ArgTypes...)>::CreateMethodInArgPtrTuple(
    const std::tuple<ArgTypes*...>& ptrs,
    int queue_index,
    std::index_sequence<I...>)
{
    return std::make_tuple(
        MethodInArgPtr<ArgTypes>(*(std::get<I>(ptrs)), this->is_in_arg_ptr_active_[queue_index][I], queue_index)...);
}

/// \brief View on ProxyMethod to provide access to internal type-erased type information.
template <typename ReturnType, typename... ArgTypes>
class ProxyMethodView
{
  public:
    explicit ProxyMethodView(ProxyMethod<ReturnType, ArgTypes...>& proxy_method) : proxy_method_{proxy_method} {}

    std::optional<TypeErasedDataTypeInfo> GetTypeErasedReturnType() const noexcept
    {
        return proxy_method_.type_erased_return_type_;
    }

    std::optional<TypeErasedDataTypeInfo> GetTypeErasedInAgs() const noexcept
    {
        return proxy_method_.type_erased_in_args_;
    }

  private:
    ProxyMethod<ReturnType, ArgTypes...>& proxy_method_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PROXY_METHOD_H
