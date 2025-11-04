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
#ifndef SCORE_MW_COM_IMPL_METHODS_PROXY_METHOD_H
#define SCORE_MW_COM_IMPL_METHODS_PROXY_METHOD_H

#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/methods/method_signature_element_ptr.h"
#include "score/mw/com/impl/methods/proxy_method_base.h"
#include "score/mw/com/impl/methods/proxy_method_binding.h"
#include "score/mw/com/impl/proxy_base.h"
#include "score/mw/com/impl/util/type_erased_storage.h"

#include "score/containers/dynamic_array.h"
#include "score/memory/shared/data_type_size_info.h"
#include "score/result/result.h"

#include <algorithm>
#include <optional>

namespace score::mw::com::impl
{

namespace detail
{

/// \brief Determines the next available queue slot in the case of a method call with in-args, where it needs to be
/// checked, whether MethodInArgPtr arguments are still active.
/// \return If there is an available queue slot, returns its index. Otherwise, returns ComErrc::kCallQueueFull.
template <typename... ArgTypes>
score::Result<std::size_t> DetermineNextAvailableQueueSlot(
    containers::DynamicArray<std::array<bool, sizeof...(ArgTypes)>>& in_arg_ptr_flags,
    containers::DynamicArray<bool>& return_type_ptr_flags)
{
    // Find an index, where both are_in_arg_ptrs_active_ and is_return_type_ptr_active_ are inactive
    for (std::size_t i = 0U; i < in_arg_ptr_flags.size(); ++i)
    {
        bool all_inactive = std::none_of(in_arg_ptr_flags[i].begin(), in_arg_ptr_flags[i].end(), [](bool active) {
            return active;
        });
        if (all_inactive && (!return_type_ptr_flags[i]))
        {
            // Found an available slot
            return i;
        }
    }
    return score::MakeUnexpected(ComErrc::kCallQueueFull);
}

/// \brief Determines the next available queue slot in the case of a method call without in-args, where only the
/// return type pointer needs to be checked.
/// \return If there is an available queue slot, returns its index. Otherwise, returns ComErrc::kCallQueueFull.
score::Result<std::size_t> DetermineNextAvailableQueueSlot(containers::DynamicArray<bool>& return_type_ptr_flags)
{
    for (std::size_t i = 0U; i < return_type_ptr_flags.size(); ++i)
    {
        if (!return_type_ptr_flags[i])
        {
            return i;
        }
    }
    return score::MakeUnexpected(ComErrc::kCallQueueFull);
}

/// \brief Creates a tuple of MethodInArgPtr for the given argument types from the given tuple of raw pointers.
/// \tparam I Compile-time index sequence for the argument types.
/// \param ptrs Tuple of raw pointers to the argument values.
/// \param in_arg_ptr_flags Dynamic array of in-argument pointer active flags.
/// \param queue_index Queue index to be set in each MethodInArgPtr.
/// \return Tuple of MethodInArgPtr for each argument type.
template <typename... ArgTypes, std::size_t... I>
std::tuple<impl::MethodInArgPtr<ArgTypes>...> CreateMethodInArgPtrTuple(
    const std::tuple<ArgTypes*...>& ptrs,
    containers::DynamicArray<std::array<bool, sizeof...(ArgTypes)>>& in_arg_ptr_flags,
    std::size_t queue_index,
    std::index_sequence<I...>)
{
    return std::make_tuple(
        MethodInArgPtr<ArgTypes>(*(std::get<I>(ptrs)), in_arg_ptr_flags[queue_index][I], queue_index)...);
}

/// \brief Allocates in-argument storage for a ProxyMethod with in-arguments. Helper method used by all ProxyMethod
/// template specializations with in-arguments.
/// \details Asserts, that the binding successfully allocated the in-argument storage, if we determined an available
/// queue slot on the binding independent level.
/// \return either a tuple of MethodInArgPtr for each argument type or an error code ComErrc::kCallQueueFull
template <typename... ArgTypes>
score::Result<std::tuple<impl::MethodInArgPtr<ArgTypes>...>> Allocate(
    ProxyMethodBinding& binding,
    containers::DynamicArray<std::array<bool, sizeof...(ArgTypes)>>& in_arg_ptr_flags,
    containers::DynamicArray<bool>& return_type_ptr_flags)
{
    auto available_queue_slot =
        detail::DetermineNextAvailableQueueSlot<ArgTypes...>(in_arg_ptr_flags, return_type_ptr_flags);
    if (!available_queue_slot.has_value())
    {
        return Unexpected(available_queue_slot.error());
    }
    const std::size_t queue_index = available_queue_slot.value();
    auto allocated_in_args_storage = binding.AllocateInArgs(queue_index);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(allocated_in_args_storage.has_value(),
                           "ProxyMethod::Allocate: AllocateInArgs failed unexpectedly.");
    score::cpp::span<std::byte> in_args_buffer{(allocated_in_args_storage.value().data()),
                                        allocated_in_args_storage.value().size()};
    const auto deserialized_arg_pointers = impl::Deserialize<ArgTypes...>(in_args_buffer);
    auto method_in_arg_ptr_tuple = detail::CreateMethodInArgPtrTuple(
        deserialized_arg_pointers, in_arg_ptr_flags, queue_index, std::make_index_sequence<sizeof...(ArgTypes)>());

    return score::Result<std::tuple<score::mw::com::impl::MethodInArgPtr<ArgTypes>...>>(std::move(method_in_arg_ptr_tuple));
}

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
std::size_t GetCommonQueuePosition(const MethodInArgPtrs&... args)
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

/// \brief Partial specialization of ProxyMethod for function signatures with no arguments and void return.
template <>
class ProxyMethod<void()> final : public ProxyMethodBase
{
    template <typename R, typename... A>
    // Design decision: This friend class provides a view on the internals of ProxyMethod.
    // This enables us to hide unnecessary internals from the end-user.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class ProxyMethodView;

  public:
    ProxyMethod(ProxyBase& proxy_base,
                std::unique_ptr<ProxyMethodBinding> proxy_method_binding,
                std::string_view method_name) noexcept
        : ProxyMethodBase(proxy_base, std::move(proxy_method_binding), method_name)
    {
    }

    /// \brief A ProxyMethod shall not be copyable. (Exactly like impl::ProxyBase and impl:ProxyEventBase)
    ProxyMethod(const ProxyMethod&) = delete;
    ProxyMethod& operator=(const ProxyMethod&) = delete;

    /// \brief A ProxyMethod shall be moveable. (Exactly like impl::ProxyBase and impl:ProxyEventBase)
    ProxyMethod(ProxyMethod&&) = default;
    ProxyMethod& operator=(ProxyMethod&&) = default;

    /// \brief This is the call-operator of ProxyMethod with no arguments and a void ReturnType.
    score::ResultBlank operator()();

  private:
    /// \brief Empty optional as in this class template specialization we do not have in-arguments.
    /// \details We still keep this member for interface consistency with the general ProxyMethod template
    /// specialization. The access via ProxyMethodView remains the same.
    static constexpr std::optional<memory::shared::DataTypeSizeInfo> type_erased_in_args_{};

    /// \brief Empty optional as in this class template specialization we do not have a return type.
    /// \details We still keep this member for interface consistency with the general ProxyMethod template
    /// specialization. The access via ProxyMethodView remains the same.
    static constexpr std::optional<memory::shared::DataTypeSizeInfo> type_erased_return_type_{};
};

/// \brief Partial specialization of ProxyMethod for function signatures with no arguments and non-void return
/// \tparam ReturnType return type of the method
template <typename ReturnType>
class ProxyMethod<ReturnType()> final : public ProxyMethodBase
{
    template <typename R, typename... A>
    // Design decision: This friend class provides a view on the internals of ProxyMethod.
    // This enables us to hide unnecessary internals from the end-user.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class ProxyMethodView;

  public:
    ProxyMethod(ProxyBase& proxy_base,
                std::unique_ptr<ProxyMethodBinding> proxy_method_binding,
                std::string_view method_name) noexcept
        : ProxyMethodBase(proxy_base, std::move(proxy_method_binding), method_name)
    {
    }

    /// \brief A ProxyMethod shall not be copyable. (Exactly like impl::ProxyBase and impl:ProxyEventBase)
    ProxyMethod(const ProxyMethod&) = delete;
    ProxyMethod& operator=(const ProxyMethod&) = delete;

    /// \brief A ProxyMethod shall be moveable. (Exactly like impl::ProxyBase and impl:ProxyEventBase)
    ProxyMethod(ProxyMethod&&) = default;
    ProxyMethod& operator=(ProxyMethod&&) = default;

    /// \brief This is the call-operator of ProxyMethod with no arguments for a non-void ReturnType.
    score::Result<MethodReturnTypePtr<ReturnType>> operator()();

  private:
    /// \brief Empty optional as in this class template specialization we do not have in-arguments.
    /// \details We still keep this member for interface consistency with the general ProxyMethod template
    /// specialization. The access via ProxyMethodView remains the same.
    static constexpr std::optional<memory::shared::DataTypeSizeInfo> type_erased_in_args_{};

    /// \brief Compile-time initialized memory::shared::DataTypeSizeInfo for the return type of this ProxyMethod.
    /// \details This is the only information about the return type of this Proxy Method, which is available at
    /// runtime. It is handed down to the binding layer, which then does the type agnostic transport.
    static constexpr std::optional<memory::shared::DataTypeSizeInfo> type_erased_return_type_ =
        CreateDataTypeSizeInfoFromTypes<ReturnType>();
};

/// \brief Partial specialization of ProxyMethod for function signatures with arguments and non-void return
/// \tparam ReturnType return type of the method
/// \tparam ArgTypes argument types of the method
template <typename ReturnType, typename... ArgTypes>
class ProxyMethod<ReturnType(ArgTypes...)> final : public ProxyMethodBase
{
    template <typename R, typename... A>
    // Design decision: This friend class provides a view on the internals of ProxyMethod.
    // This enables us to hide unnecessary internals from the end-user.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class ProxyMethodView;

  public:
    ProxyMethod(ProxyBase& proxy_base,
                std::unique_ptr<ProxyMethodBinding> proxy_method_binding,
                std::string_view method_name) noexcept
        : ProxyMethodBase(proxy_base, std::move(proxy_method_binding), method_name),
          are_in_arg_ptrs_active_{kCallQueueSize}
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
    score::Result<std::tuple<impl::MethodInArgPtr<ArgTypes>...>> Allocate()
    {
        return detail::Allocate<ArgTypes...>(*binding_, are_in_arg_ptrs_active_, is_return_type_ptr_active_);
    };

    /// \brief This is the copying call-operator of ProxyMethod for a non-void ReturnType.
    /// \details This call-operator variant takes the argument values as references. It then internally calls Allocate()
    /// to get the needed storage for the arguments and return value and copies the arguments into the allocated
    /// storage.
    score::Result<MethodReturnTypePtr<ReturnType>> operator()(const ArgTypes&... args);

    /// \brief This is the zero-copy call-operator of ProxyMethod for a non-void ReturnType.
    /// \details This call-operator variant takes the argument values as MethodInArgPtr, i.e. as pointers to the
    /// argument values, which have been allocated before via Allocate() call.
    score::Result<MethodReturnTypePtr<ReturnType>> operator()(MethodInArgPtr<ArgTypes>... args);

  private:
    /// \brief Compile-time initialized memory::shared::DataTypeSizeInfo for the argument types of this ProxyMethod.
    /// \details This is the only information about the argument types of this Proxy Method, which is available at
    /// runtime. It is handed down to the binding layer, which then does the type agnostic transport.
    /// \remark If there are no arguments, this is std::nullopt.
    static constexpr std::optional<memory::shared::DataTypeSizeInfo> type_erased_in_args_ =
        CreateDataTypeSizeInfoFromTypes<ArgTypes...>();

    /// \brief Compile-time initialized memory::shared::DataTypeSizeInfo for the return type of this ProxyMethod.
    /// \details This is the only information about the return type of this Proxy Method, which is available at
    /// runtime. It is handed down to the binding layer, which then does the type agnostic transport.
    static constexpr std::optional<memory::shared::DataTypeSizeInfo> type_erased_return_type_ =
        CreateDataTypeSizeInfoFromTypes<ReturnType>();

    /// \brief Outer dynamic array: one entry per call-queue position, inner array: one entry per argument.
    /// \details This array of arrays contains bool flags, which indicate, if the corresponding argument pointer
    /// passed to the zero-copy call-operator is active (true) or not (false).
    /// E.g. are_in_arg_ptrs_active_[0][2] == true means, that for the call-queue position 0, the 3rd argument
    /// pointer passed to the zero-copy call-operator is active.
    containers::DynamicArray<std::array<bool, sizeof...(ArgTypes)>> are_in_arg_ptrs_active_;
};

/// \brief Partial specialization of ProxyMethod for function signatures with arguments and void return
/// \tparam ArgTypes argument types of the method
template <typename... ArgTypes>
class ProxyMethod<void(ArgTypes...)> final : public ProxyMethodBase
{
    template <typename R, typename... A>
    // Design decision: This friend class provides a view on the internals of ProxyMethod.
    // This enables us to hide unnecessary internals from the end-user.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class ProxyMethodView;

  public:
    ProxyMethod(ProxyBase& proxy_base,
                std::unique_ptr<ProxyMethodBinding> proxy_method_binding,
                std::string_view method_name) noexcept
        : ProxyMethodBase(proxy_base, std::move(proxy_method_binding), method_name),
          are_in_arg_ptrs_active_{kCallQueueSize}
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
    score::Result<std::tuple<impl::MethodInArgPtr<ArgTypes>...>> Allocate()
    {
        return detail::Allocate<ArgTypes...>(*binding_, are_in_arg_ptrs_active_, is_return_type_ptr_active_);
    };

    /// \brief This is the copying call-operator of ProxyMethod for a void ReturnType.
    /// \details This call-operator variant takes the argument values as references.
    score::ResultBlank operator()(const ArgTypes&... args);

    /// \brief This is the zero-copy call-operator of ProxyMethod for a void ReturnType. It only exists if there is at
    /// least one argument.
    /// \details This call-operator variant takes the argument values as MethodInArgPtr, i.e. as pointers to the
    /// argument values, which have been allocated before via Allocate() call.
    score::ResultBlank operator()(MethodInArgPtr<ArgTypes>... args);

  private:
    /// \brief Compile-time initialized memory::shared::DataTypeSizeInfo for the argument types of this ProxyMethod.
    /// \details This is the only information about the argument types of this Proxy Method, which is available at
    /// runtime. It is handed down to the binding layer, which then does the type agnostic transport.
    static constexpr std::optional<memory::shared::DataTypeSizeInfo> type_erased_in_args_ =
        CreateDataTypeSizeInfoFromTypes<ArgTypes...>();

    /// \brief Empty optional as in this class template specialization we do not have a return type.
    /// \details We still keep this member for interface consistency with the general ProxyMethod template
    /// specialization. The access via ProxyMethodView remains the same.
    static constexpr std::optional<memory::shared::DataTypeSizeInfo> type_erased_return_type_{};

    /// \brief Outer dynamic array: one entry per call-queue position, inner array: one entry per argument.
    /// \details This array of arrays contains bool flags, which indicate, if the corresponding argument pointer
    /// passed to the zero-copy call-operator is active (true) or not (false).
    /// E.g. are_in_arg_ptrs_active_[0][2] == true means, that for the call-queue position 0, the 3rd argument
    /// pointer passed to the zero-copy call-operator is active.
    containers::DynamicArray<std::array<bool, sizeof...(ArgTypes)>> are_in_arg_ptrs_active_;
};

/// ---------------------------------------------------------------------------------------------
/// Implementation of ProxyMethod class template specialization with empty arguments and void return
/// ------------------------------------------------------------------------------------------------

score::ResultBlank ProxyMethod<void()>::operator()()
{
    auto queue_position = detail::DetermineNextAvailableQueueSlot(is_return_type_ptr_active_);
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

/// ---------------------------------------------------------------------------------------------
/// Implementation of ProxyMethod class template specialization with empty arguments and non-void return
/// ------------------------------------------------------------------------------------------------

template <typename ReturnType>
score::Result<MethodReturnTypePtr<ReturnType>> ProxyMethod<ReturnType()>::operator()()
{
    auto queue_position_result = detail::DetermineNextAvailableQueueSlot(is_return_type_ptr_active_);
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

/// ---------------------------------------------------------------------------------------------
/// Implementation of ProxyMethod class template specialization with arguments and non-void return
/// ------------------------------------------------------------------------------------------------

template <typename ReturnType, typename... ArgTypes>
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
score::Result<MethodReturnTypePtr<ReturnType>> ProxyMethod<ReturnType(ArgTypes...)>::operator()(
    MethodInArgPtr<ArgTypes>... args)
{
    auto queue_position = detail::GetCommonQueuePosition(args...);
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

/// ---------------------------------------------------------------------------------------------
/// Implementation of ProxyMethod class template specialization with arguments and void return
/// ------------------------------------------------------------------------------------------------

template <typename... ArgTypes>
score::ResultBlank ProxyMethod<void(ArgTypes...)>::operator()(const ArgTypes&... args)
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

template <typename... ArgTypes>
score::ResultBlank ProxyMethod<void(ArgTypes...)>::operator()(MethodInArgPtr<ArgTypes>... args)
{
    auto queue_position = detail::GetCommonQueuePosition(args...);
    // \todo (Ticket-221600): Clarify stop_token usage
    auto call_result = binding_->DoCall(queue_position, score::cpp::stop_token{});
    if (!call_result.has_value())
    {
        return Unexpected(call_result.error());
    }
    return {};
}

/// \brief View on ProxyMethod to provide access to internal type-erased type information.
template <typename ReturnType, typename... ArgTypes>
class ProxyMethodView
{
  public:
    explicit ProxyMethodView(ProxyMethod<ReturnType(ArgTypes...)>& proxy_method) : proxy_method_{proxy_method} {}

    std::optional<memory::shared::DataTypeSizeInfo> GetTypeErasedReturnType() const noexcept
    {
        return proxy_method_.type_erased_return_type_;
    }

    std::optional<memory::shared::DataTypeSizeInfo> GetTypeErasedInAgs() const noexcept
    {
        return proxy_method_.type_erased_in_args_;
    }

  private:
    ProxyMethod<ReturnType(ArgTypes...)>& proxy_method_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_METHODS_PROXY_METHOD_H
