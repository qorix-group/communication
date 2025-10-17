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
    /// \brief Allocates the necessary storage for the argument values and the return value of a method call.
    /// \return On success, a tuple of MethodInArgPtr for each argument type is returned. On failure, an error code is
    /// returned.
    [[nodiscard]] score::Result<std::tuple<impl::MethodInArgPtr<ArgTypes>...>> Allocate()
    {
        return {};
    }

    /// \brief This is the non-zero-copy call-operator of ProxyMethod for a non-void ReturnType.
    /// \details This call-operator variant takes the argument values as references. It then internally calls Allocate()
    /// to get the needed storage for the arguments and return value and copies the arguments into the allocated
    /// storage.
    template <typename R = ReturnType>
    typename std::enable_if<!std::is_void<R>::value, score::Result<MethodReturnTypePtr<R>>>::type operator()(
        ArgTypes&... args) const
    {
        // Implementation would go here
        // We need to call Allocate() first to get the needed storage for the arguments and return value!. Then we
        // would copy the args into the allocated storage and call the remote method.
        return MethodReturnTypePtr<R>{};
    }

    /// \brief This is the zero-copy call-operator of ProxyMethod for a non-void ReturnType.
    /// \details This call-operator variant takes the argument values as MethodInArgPtr, i.e. as pointers to the
    /// argument values, which have been allocated before via Allocate() call.
    template <typename R = ReturnType>
    typename std::enable_if<!std::is_void<R>::value, score::Result<MethodReturnTypePtr<R>>>::type operator()(
        MethodInArgPtr<ArgTypes>... args) const
    {
        // Implementation would go here
        return MethodReturnTypePtr<R>{};
    }

    /// \brief This is the non-zero-copy call-operator of ProxyMethod for a void ReturnType.
    /// \details This call-operator variant takes the argument values as references.
    template <typename R = ReturnType>
    typename std::enable_if<std::is_void<R>::value, score::ResultBlank>::type operator()(ArgTypes&... args) const
    {
        // Implementation would go here
        // No return for void
    }

    /// \brief This is the zero-copy call-operator of ProxyMethod for a void ReturnType.
    /// \details This call-operator variant takes the argument values as MethodInArgPtr, i.e. as pointers to the
    /// argument values, which have been allocated before via Allocate() call.
    template <typename R = ReturnType>
    typename std::enable_if<std::is_void<R>::value, score::ResultBlank>::type operator()(
        MethodInArgPtr<ArgTypes>... args) const
    {
        // Implementation would go here
        // No return for void
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

    /// \brief Outer dynamic array: one entry per call-queue position, inner array: one entry per argument.
    /// \details This array of arrays contains bool flags, which indicate, if the corresponding argument pointer
    /// passed to the zero-copy call-operator is active (true) or not (false).
    /// E.g. is_in_arg_ptr_active_[0][2] == true means, that for the call-queue position 0, the 3rd argument
    /// pointer passed to the zero-copy call-operator is active.
    containers::DynamicArray<std::array<bool, sizeof...(ArgTypes)>> is_in_arg_ptr_active_;
    /// \brief Dynamic array: one entry per call-queue position.
    /// \details This array contains bool flags, which indicate, if the return value pointer
    /// passed to the zero-copy call-operator is active (true) or not (false).
    /// The optionals are needed, since ProxyMethods without return value don't have a return type pointer.
    containers::DynamicArray<std::optional<bool>> is_return_type_ptr_active_;
};

}  // namespace score::mw::com::impl

#endif
