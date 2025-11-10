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
#ifndef SCORE_MW_COM_IMPL_SKELETON_METHOD_H
#define SCORE_MW_COM_IMPL_SKELETON_METHOD_H

#include "score/result/result.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/com/impl/skeleton_method_base.h"
#include "score/mw/com/impl/skeleton_method_binding.h"
#include "score/mw/com/impl/util/type_erased_storage.h"

#include <functional>
#include <memory>
#include <optional>
#include <tuple>

#include <score/span.hpp>

namespace score::mw::com::impl
{

template <typename Signature>
class SkeletonMethod;  // undefined - if we end up here, someone tried to use an unsupported signature for a
                       // ProxyMethod!

template <typename ReturnType, typename... ArgTypes>
class SkeletonMethod<ReturnType(ArgTypes...)> final : public SkeletonMethodBase
{
  public:
    /// \brief testonly constructor, which allows for direct injection of a mock binding
    SkeletonMethod(SkeletonBase& skeleton_base,
                   const std::string_view method_name,
                   std::unique_ptr<SkeletonMethodBinding> skeleton_method_binding)
        : SkeletonMethodBase(skeleton_base, method_name, std::move(skeleton_method_binding))
    {
        auto skeleton_base_view = SkeletonBaseView{skeleton_base};
        skeleton_base_view.RegisterMethod(method_name_, *this);
    }
    using MethodType = ReturnType(ArgTypes...);
    // using Callable = score::cpp::callback<MethodType>;

    ~SkeletonMethod() = default;

    SkeletonMethod(const SkeletonMethod&) = delete;
    SkeletonMethod& operator=(const SkeletonMethod&) & = delete;

    SkeletonMethod(SkeletonMethod&& other) noexcept : SkeletonMethodBase(std::move(other))
    {
        SkeletonBaseView skeleton_base_view{skeleton_base_.get()};
        skeleton_base_view.UpdateMethod(method_name_, *this);
    }

    SkeletonMethod& operator=(SkeletonMethod&& other) & noexcept
    {
        if (this != &other)
        {
            SkeletonMethodBase::operator=(std::move(other));
            SkeletonBaseView skeleton_base_view{skeleton_base_.get()};
            skeleton_base_view.UpdateMethod(method_name_, *this);
        }
        return *this;
    }

    /// \brief Register a callback with the binding, which will be expecuted by the binding when the Proxy calls this
    /// method.
    /// \return score::cpp::blank on success and ComErrc::kMethodNotExisting on failiure
    // template<typename Callable>
    // using MethodType = ReturnType(ArgTypes...); void(int, bool)
    // using Callable = score::cpp::callback<MethodType>;
    template <typename Callable>
    ResultBlank Register(Callable&& callback)
    {
        auto callable_invoker = [callable = std::move(callback)](auto&&... ptrs) -> decltype(auto) {
            return std::invoke(callable, (*ptrs)...);
        };

        SkeletonMethodBinding::TypeErasedCallback type_erased_callable =
            [callable_invoker = std::move(callable_invoker)](std::optional<score::cpp::span<std::byte>> type_erased_result,
                                                             std::optional<score::cpp::span<std::byte>> type_erased_in_args) {
                using InArgsTupleType = std::tuple<ArgTypes*...>;
                InArgsTupleType typed_in_args{};

                constexpr bool is_empty_pack = (sizeof...(ArgTypes) == 0);

                if constexpr (!is_empty_pack)
                {
                    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(type_erased_in_args.has_value(),
                                           "ArgTypes is non void. Thus, type_erased_in_args needs to have a value!");
                    typed_in_args = Deserialize<ArgTypes...>(type_erased_in_args.value());
                }

                if constexpr (!std::is_same_v<ReturnType, void>)
                {
                    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(type_erased_result.has_value(),
                                           "ReturnType is non void. Thus, type_erased_result needs to have a value!");
                    ReturnType res = std::apply(callable_invoker, std::forward<InArgsTupleType>(typed_in_args));
                    SerializeArgs<ReturnType>(type_erased_result.value(), res);
                }
                else
                {
                    std::apply(callable_invoker, std::forward<InArgsTupleType>(typed_in_args));
                }
            };

        return binding_->Register(std::move(type_erased_callable));
    }

    void UpdateSkeletonReference(SkeletonBase& skeleton_base) noexcept
    {
        skeleton_base_ = skeleton_base;
    }
};

}  // namespace score::mw::com::impl
#endif  // SCORE_MW_COM_IMPL_SKELETON_METHOD_H
