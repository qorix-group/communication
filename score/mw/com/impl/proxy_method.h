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
#ifndef SCORE_MW_COM_IMPL_PROXMETHOD_H
#define SCORE_MW_COM_IMPL_PROXMETHOD_H

#include "score/containers/dynamic_array.h"
#include "score/result/result.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/method_signature_element_ptr.h"
#include "score/mw/com/impl/proxy_base.h"
#include "score/mw/com/impl/proxy_method_binding.h"
#include "score/mw/com/impl/util/type_erased_storage.h"

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

}  // namespace detail

template <typename Signature>
class ProxyMethod;  // undefined - if we end up here, someone tried to use an unsupported signature for a ProxyMethod!

template <typename ReturnType, typename... ArgTypes>
class ProxyMethod<ReturnType(ArgTypes...)> final
{
  public:
    ProxyMethod(ProxyBase& /* proxy_base */,
                std::unique_ptr<ProxyMethodBinding> proxy_method_binding,
                std::string_view method_name) noexcept
        : method_name_{method_name}, binding_{std::move(proxy_method_binding)}
    {
    }

    /// \brief Allocates the necessary storage for the argument values and the return value of a method call.
    /// \return On success, a tuple of MethodInArgPtr for each argument type is returned. On failure, an error code is
    /// returned.
    template <typename Dummy = void>
    typename std::enable_if<(sizeof...(ArgTypes) > 0), score::Result<std::tuple<impl::MethodInArgPtr<ArgTypes>...>>>::type
    Allocate()
    {
        auto availableQueueSlot = AllocateNextAvailableQueueSlot();
        if (!availableQueueSlot.has_value())
        {
            return Unexpected(availableQueueSlot.error());
        }
        const int queue_index = availableQueueSlot.value();
        auto allocatedInArgsStorage = binding_->AllocateInArgs(queue_index);
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(allocatedInArgsStorage.has_value(),
                               "ProxyMethod::Allocate: AllocateInArgs failed unexpectedly.");
        MemoryBufferAccessor in_args_buffer_accessor{reinterpret_cast<std::byte*>((allocatedInArgsStorage.value())),
                                                     type_erased_in_args_.value().size};
        const auto deserialized_arg_pointers = Deserialize<ArgTypes...>(in_args_buffer_accessor);
        // Now create the MethodInArgPtr instances for each argument pointer, thereby also setting the is_active flags!
        auto method_in_arg_ptr_tuple = CreateMethodInArgPtrTuple(
            deserialized_arg_pointers,
            queue_index,
            std::make_index_sequence<sizeof...(ArgTypes)>()
        );

        return score::Result<std::tuple<impl::MethodInArgPtr<ArgTypes>...>>(std::move(method_in_arg_ptr_tuple));
    }

    /// \brief This is the copying call-operator of ProxyMethod for a non-void ReturnType.
    /// \details This call-operator variant takes the argument values as references. It then internally calls Allocate()
    /// to get the needed storage for the arguments and return value and copies the arguments into the allocated
    /// storage.
    template <typename R = ReturnType>
    typename std::enable_if<!std::is_void<R>::value, score::Result<MethodReturnTypePtr<R>>>::type operator()(
        ArgTypes&... args)
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
                return operator()(std::move(in_args_ptrs)...);
            },
            in_arg_ptr_tuple
        );
    }

    /// \brief This is the zero-copy call-operator of ProxyMethod for a non-void ReturnType.
    /// \details This call-operator variant takes the argument values as MethodInArgPtr, i.e. as pointers to the
    /// argument values, which have been allocated before via Allocate() call.
    template <typename R = ReturnType>
    typename std::enable_if<!std::is_void<R>::value, score::Result<MethodReturnTypePtr<R>>>::type operator()(
        MethodInArgPtr<ArgTypes>... args)
    {
        auto queue_position = AssertSameQueuePosition(args...);
        auto allocatedReturnTypeStorage = binding_->AllocateReturnType(static_cast<std::size_t>(queue_position));
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(allocatedReturnTypeStorage.has_value(),
                               "ProxyMethod::operator(): AllocateReturnType failed unexpectedly.");
        auto call_result = binding_->DoCall(static_cast<std::size_t>(queue_position), score::cpp::stop_token{});
        if (!call_result.has_value())
        {
            return Unexpected(call_result.error());
        }
        return MethodReturnTypePtr<R>{static_cast<R*>(allocatedReturnTypeStorage.value()),
                                      is_return_type_ptr_active_[queue_position].value(),
                                      queue_position};
    }

    /// \brief This is the copying call-operator of ProxyMethod for a void ReturnType.
    /// \details This call-operator variant takes the argument values as references.
    template <typename R = ReturnType>
    typename std::enable_if<std::is_void<R>::value, score::ResultBlank>::type operator()(ArgTypes&... args)
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
            in_arg_ptr_tuple
        );
    }

    /// \brief This is the zero-copy call-operator of ProxyMethod for a void ReturnType. It only exists if there is at
    /// least one argument.
    /// \details This call-operator variant takes the argument values as MethodInArgPtr, i.e. as pointers to the
    /// argument values, which have been allocated before via Allocate() call.
    template <typename R = ReturnType, typename Dummy = void>
    typename std::enable_if<(sizeof...(ArgTypes) > 0) && std::is_void<R>::value, score::ResultBlank>::type operator()(
        MethodInArgPtr<ArgTypes>... args)
    {
        auto queue_position = AssertSameQueuePosition(args...);
        auto call_result = binding_->DoCall(static_cast<std::size_t>(queue_position), score::cpp::stop_token{});
        if (!call_result.has_value())
        {
            return Unexpected(call_result.error());
        }
        return {};
    }

  private:
    score::Result<int> AllocateNextAvailableQueueSlot()
    {
        // Find an index, where in is_in_arg_ptr_active_ and is_return_type_ptr_active_ is inactive
        for (std::size_t i = 0; i < is_in_arg_ptr_active_.size(); ++i)
        {
            bool all_inactive = true;
            for (bool active : is_in_arg_ptr_active_[i])
            {
                if (active)
                {
                    all_inactive = false;
                    break;
                }
            }
            if (all_inactive && (!is_return_type_ptr_active_[i].has_value() || !is_return_type_ptr_active_[i].value()))
            {
                // Found an available slot
                return static_cast<int>(i);
            }
        }
        return score::MakeUnexpected(ComErrc::kCallQueueFull);
    }

    /// \brief Asserts that all MethodInArgPtr arguments have the same queue_position_.
    /// \tparam MethodInArgPtrs Variadic template parameter pack for MethodInArgPtr types.
    /// \param args MethodInArgPtr arguments to check.
    /// \return The common queue_position_ value.
    ///
    /// \remark We are not checking, whether the user handed over MethodInArgPtr from different ProxyMethod
    /// instances. This would require more type information at runtime, which we don't have. Therefore, we just check,
    /// that all queue_position_ values are the same.
    template <typename... MethodInArgPtrs>
    static int AssertSameQueuePosition(const MethodInArgPtrs&... args)
    {
        int expected_queue_position = -1;
        (([&expected_queue_position](const auto& arg) {
             if (expected_queue_position == -1)
             {
                 expected_queue_position = arg.GetQueuePosition();
             }
             else
             {
                 if (arg.GetQueuePosition() != static_cast<std::size_t>(expected_queue_position))
                 {
                     SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(false, "All MethodInArgPtr arguments must have the same queue_position_");
                 }
             }
         })(args),
         ...);
        return expected_queue_position;
    }

    template <std::size_t... I>
    std::tuple<impl::MethodInArgPtr<ArgTypes>...> CreateMethodInArgPtrTuple(
        const std::tuple<ArgTypes*...>& ptrs,
        int queue_index,
        std::index_sequence<I...> = std::make_index_sequence<sizeof...(ArgTypes)>())
    {
        return std::make_tuple(
            MethodInArgPtr<ArgTypes>(std::get<I>(ptrs), this->is_in_arg_ptr_active_[queue_index][I], queue_index)...);
    }

    /// \brief Compile-time initialized TypeErasedDataTypeInfo for the argument types of this ProxyMethod.
    /// \details This is the only information about the argument types of this Proxy Method, which is available at
    /// runtime. It is handed down to the binding layer, which then does the type agnostic transport.
    /// \remark If there are no arguments, this is std::nullopt.
    constexpr static std::optional<TypeErasedDataTypeInfo> type_erased_in_args_ =
        detail::InitTypeErasedInArgs<ArgTypes...>();

    /// \brief Compile-time initialized TypeErasedDataTypeInfo for the return type of this ProxyMethod.
    /// \details This is the only information about the return type of this Proxy Method, which is available at
    /// runtime. It is handed down to the binding layer, which then does the type agnostic transport.
    /// \remark If the return type is void, this is std::nullopt.
    constexpr static std::optional<TypeErasedDataTypeInfo> type_erased_return_type_ =
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
    containers::DynamicArray<std::array<bool, sizeof...(ArgTypes)>> is_in_arg_ptr_active_{kCallQueueSize};
    /// \brief Dynamic array: one entry per call-queue position.
    /// \details This array contains bool flags, which indicate, if the return value pointer
    /// passed to the zero-copy call-operator is active (true) or not (false).
    /// The optionals are needed, since ProxyMethods without return value don't have a return type pointer.
    containers::DynamicArray<std::optional<bool>> is_return_type_ptr_active_{kCallQueueSize};

    std::unique_ptr<ProxyMethodBinding> binding_;
};

}  // namespace score::mw::com::impl

#endif
