
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

#ifndef SCORE_MW_COM_IMPL_RUST_GENERIC_BRIDGE_MACROS_H
#define SCORE_MW_COM_IMPL_RUST_GENERIC_BRIDGE_MACROS_H

#include "score/mw/com/impl/plumbing/sample_ptr.h"
#include "score/mw/com/impl/proxy_event.h"
#include "score/mw/com/impl/rust/proxy_bridge.h"
#include "score/mw/com/impl/skeleton_event.h"
#include "score/mw/com/types.h"

#include <score/assert.hpp>

namespace score::mw::com::impl::rust
{

/**
 * Get samples from ProxyEvent with type erasure
 * This takes callback as FatPtr and invokes the callback for each sample
 * of type T
 * @param proxy_event: reference to ProxyEvent of type T
 * @param callback: FatPtr of Rust FnMut callable that takes SamplePtr<T>
 * @param max_num_samples: maximum number of samples to process
 * @return the number of samples processed in a Result<uint32_t>
 */
template <typename T>
inline score::Result<std::uint32_t> GetSamplesFromEvent(::score::mw::com::impl::ProxyEvent<T>& proxy_event,
                                                      const FatPtr& callback,
                                                      std::uint32_t max_num_samples) noexcept
{
    RustFnMutCallable<RustRefMutCallable, void, SamplePtr<T>> rust_callable{callback};

    auto samples = proxy_event.GetNewSamples(std::move(rust_callable), max_num_samples);

    if (samples.has_value())
    {
        return samples;
    }
    else
    {
        return score::MakeUnexpected<std::uint32_t>(samples.error());
    }
}

/*
 * This is interface for type operations
 * It provides type-erased operations for types used in ProxyEvent and SkeletonEvent
 */
class ITypeOperations
{

  public:
    virtual ~ITypeOperations() = default;

    /**
     * Get samples from ProxyEvent of specific type
     * It takes ProxyEventBase pointer, max sample count and callback as FatPtr
     * @param event_ptr: pointer to ProxyEventBase
     * @param max_sample: maximum number of samples to process
     * @param callBack: FatPtr of Rust FnMut callable that takes SamplePtr<T>
     * @return the number of samples processed in a Result<uint32_t>
     */
    virtual Result<uint32_t> GetSamplesFromEvent(ProxyEventBase* event_ptr, uint32_t max_sample, FatPtr callBack) = 0;

    /**
     * Send event data through SkeletonEvent of specific type
     * It takes SkeletonEventBase pointer and data pointer
     * @param data_ptr: is a pointer to type T but erased as void* and in implementation it is casted back to T*
     */
    virtual void SkeletonSendEvent(SkeletonEventBase* event_ptr, void* data_ptr) = 0;
};

/*
 * Template implementation of ITypeOperations for type T
 * It provides implementation for GetSamplesFromEvent and SkeletonSendEvent
 * for type T
 * This class is used to register type operations for each type T from macros
 */
template <typename T>
class TypeOperationImpl : public ITypeOperations
{
  public:
    Result<uint32_t> GetSamplesFromEvent(ProxyEventBase* event_ptr, uint32_t max_sample, FatPtr callBack) override
    {
        auto proxy_event = dynamic_cast<ProxyEvent<T>*>(event_ptr);
        if (proxy_event == nullptr)
        {
            return score::MakeUnexpected<std::uint32_t>(ComErrc::kInvalidHandle);
        }
        return score::mw::com::impl::rust::GetSamplesFromEvent<T>(*proxy_event, callBack, max_sample);
    }

    void SkeletonSendEvent(SkeletonEventBase* event_ptr, void* data_ptr) override
    {

        auto skeleton_event = dynamic_cast<SkeletonEvent<T>*>(event_ptr);
        if (skeleton_event == nullptr)
        {
            return;
        }
        // Cast data_ptr back to T*
        T* typed_data = static_cast<T*>(data_ptr);
        if (typed_data == nullptr)
        {
            return;
        }
        skeleton_event->Send(*typed_data);
    }
};

/*
 * Interface for member operations for services with events,methods and fields
 * It provides type-erased access to ProxyEvent and SkeletonEvent members
 */
class IMemberOperation
{
  public:
    virtual ~IMemberOperation() = default;

    /**
     * Get ProxyEvent member from ProxyBase pointer
     * @param proxy_ptr: pointer to ProxyBase
     * @return pointer to ProxyEventBase if found, nullptr otherwise
     */
    virtual ProxyEventBase* GetProxyEvent(ProxyBase* proxy_ptr) = 0;

    /**
     * Get SkeletonEvent member from SkeletonBase pointer
     * @param skeleton_ptr: pointer to SkeletonBase
     * @return pointer to SkeletonEventBase if found, nullptr otherwise
     */
    virtual SkeletonEventBase* GetSkeletonEvent(SkeletonBase* skeleton_ptr) = 0;
};

/*
 * Template implementation of IMemberOperation for specific ProxyType and SkeletonType
 * takes event member as template parameters
 * It provides implementation for GetProxyEvent and GetSkeletonEvent
 * This call is used to register member operations for each operation from macros
 */
template <typename ProxyType,
          typename SkeletonType,
          typename EventType,
          auto proxy_event_member,
          auto skeleton_event_member>
class MemberOprationImpl : public IMemberOperation
{
  public:
    ProxyEventBase* GetProxyEvent(ProxyBase* proxy_ptr) override
    {
        auto proxy = dynamic_cast<ProxyType*>(proxy_ptr);
        if (proxy == nullptr)
        {
            return nullptr;
        }

        return static_cast<ProxyEventBase*>(&(proxy->*proxy_event_member));
    }

    SkeletonEventBase* GetSkeletonEvent(SkeletonBase* skeleton_ptr) override
    {
        if (skeleton_ptr == nullptr)
        {
            return nullptr;
        }

        auto* skeleton = dynamic_cast<SkeletonType*>(skeleton_ptr);
        if (skeleton == nullptr)
        {
            return nullptr;
        }

        return static_cast<SkeletonEventBase*>(&(skeleton->*skeleton_event_member));
    }
};

/*
 * Interface for interface operations
 * It provides type-erased operations for creating Proxy and Skeleton
 * and offering/stopping service
 * It include the MemberOperation registry for events,methods and fields
 */
class IInterfaceOperations
{
  public:
    using MemberOperationMap = std::unordered_map<std::string_view, std::shared_ptr<IMemberOperation>>;

    virtual ~IInterfaceOperations() = default;

    /**
     * Create Proxy instance from HandleType
     * @param handle_ptr Opaque handle pointer
     * @return ProxyBase pointer
     */
    virtual ProxyBase* CreateProxy(const HandleType& handle_ptr) = 0;

    /**
     * Create Skeleton instance from InstanceSpecifier
     * @param instance_specifier InstanceSpecifier object
     * @return SkeletonBase pointer
     */
    virtual SkeletonBase* CreateSkeleton(const ::score::mw::com::InstanceSpecifier& instance_specifier) = 0;

    /**
     * Offer service for SkeletonBase pointer
     * @param handle_ptr SkeletonBase pointer
     * @return true if service is offered successfully, false otherwise
     */
    virtual bool OfferService(SkeletonBase* handle_ptr) = 0;

    /**
     * Stop offering service for SkeletonBase pointer
     * @param handle_ptr SkeletonBase pointer
     */
    virtual void StopOfferService(SkeletonBase* handle_ptr) = 0;

    /**
     * Delete the proxy instance
     * @param proxy_ptr pointer to proxy instance
     */
    virtual void DestroyProxy(ProxyBase* proxy_ptr) = 0;

    /**
     * Delete the skeleton instance
     * @param skeleton_ptr pointer to skeleton instance
     */
    virtual void DestroySkeleton(SkeletonBase* skeleton_ptr) = 0;

    /**
     * Register member operation for event,method or field
     * As of now it is only used for events
     * @param member_name name of the event used as key in member operation map
     * @param ops pointer to IMemberOperation implementation
     */
    void RegisterMemberOperation(const std::string_view& member_name, std::shared_ptr<IMemberOperation> ops)
    {
        GetMemberOperationMap()[member_name] = ops;
    }

    /**
     * Get member operation for event,method or field
     * As of now it is only used for events
     * @param member_name name of the event used as key in member operation map
     */
    IMemberOperation* GetMemberOperation(const std::string_view& member_name)
    {
        auto it = GetMemberOperationMap().find(member_name);
        if (it != GetMemberOperationMap().end())
        {
            return it->second.get();
        }
        return nullptr;
    }

  private:
    MemberOperationMap& GetMemberOperationMap()
    {
        static MemberOperationMap s_member_operation_map;
        return s_member_operation_map;
    }

    // TODO: Add if needed
    //  virtual bool IsProxyRegistered() = 0;
    //  virtual bool IsSkeletonRegistered() = 0;
};

/*
 * Template implementation of IInterfaceOperations for specific Proxy and Skeleton types
 * It provides implementation for CreateProxy, CreateSkeleton, OfferService and StopOfferService
 * This class is used to register interface operations for each interface from macros
 */
template <typename AsProxy, typename AsSkeleton>
class InterfaceOperationImpl : public IInterfaceOperations
{
  public:
    ProxyBase* CreateProxy(const HandleType& handle_ptr) override
    {

        try
        {
            // Pass the dereferenced reference directly
            auto result = AsProxy::Create(handle_ptr);

            if (result.has_value())
            {
                AsProxy* proxy = new AsProxy{std::move(result).value()};
                return static_cast<ProxyBase*>(proxy);
            }
            else
            {
                return nullptr;
            }
        }
        catch (const std::bad_alloc& e)
        {
            // TODO: Log the exception
            // std::cerr<<"CPP Macro # Caught std::bad_alloc in CreateProxy: " << e.what() << std::endl;
            return nullptr;
        }
        catch (const std::exception& e)
        {
            // TODO: Log the exception
            // std::cerr<<"CPP Macro # Caught exception in CreateProxy: " << e.what() << std::endl;
            return nullptr;
        }
    }

    SkeletonBase* CreateSkeleton(const ::score::mw::com::InstanceSpecifier& instance_specifier) override
    {
        if (auto result = AsSkeleton::Create(instance_specifier); result.has_value())
        {
            return static_cast<SkeletonBase*>(new AsSkeleton{std::move(result).value()});
        }
        else
        {
            return nullptr;
        }
    }

    bool OfferService(SkeletonBase* handle_ptr) override
    {
        if (handle_ptr == nullptr)
        {
            return false;
        }

        bool result = handle_ptr->OfferService().has_value();
        return result;
    }

    void StopOfferService(SkeletonBase* handle_ptr) override
    {
        handle_ptr->StopOfferService();
    }

    void DestroyProxy(ProxyBase* proxy_ptr) override
    {
        delete proxy_ptr;
    }

    void DestroySkeleton(SkeletonBase* skeleton_ptr) override
    {
        delete skeleton_ptr;
    }
};

/**
 * Global event registry mapping
 * It provides global registry for interface operations and type operations
 * It is used to register and find interface operations and type operations
 * Called by macros to register interface and type operations
 * Also used by FFI layer to find interface and type operations
 * For interface operations, it provides methods to register and find interface operations by interface id as key
 * For type operations, it provides methods to register and find type operations by type name as key
 */
class GlobalRegistryMapping
{
  public:
    using InterfaceOprationMap = std::unordered_map<std::string_view, std::shared_ptr<IInterfaceOperations>>;

    using TypeOperationMap = std::unordered_map<std::string_view, std::shared_ptr<ITypeOperations>>;

    /**
     * Get type operation map
     * it create static map on first call and returns reference to it
     * @return reference to type operation map
     */
    static TypeOperationMap& GetTypeOperationMap()
    {
        static TypeOperationMap s_type_operation_map;
        return s_type_operation_map;
    }

    /**
     * Register type operation for specific type name
     * Called by EXPORT_MW_COM_TYPE macro
     * @param type_name name of the type used as key in type operation map
     * @param impl pointer to ITypeOperations implementation
     */
    static void RegisterTypeOperation(const std::string_view& type_name, std::shared_ptr<ITypeOperations> impl)
    {
        GetTypeOperationMap()[type_name] = impl;
    }

    /**
     * Get interface operation map
     * it create static map on first call and returns reference to it
     * @return reference to interface operation map
     */
    static InterfaceOprationMap& GetInterfaceFactories()
    {
        static InterfaceOprationMap s_factories;
        return s_factories;
    }

    /**
     * Register event operation for specific interface id and event name
     * Called by EXPORT_MW_COM_EVENT macro
     * @param interface_id id of the interface used to find the interface operation
     * @param member_name name of the event used as key in member operation map
     * @param ops pointer to IMemberOperation implementation
     */
    static void RegisterMemberOperation(const std::string_view& interface_id,
                                        const std::string_view& member_name,
                                        std::shared_ptr<IMemberOperation> ops)
    {
        const auto& registries = GetInterfaceOperation(interface_id);
        if (registries)
        {
            registries->RegisterMemberOperation(member_name, ops);
        }
    }

    /**
     * Register interface operation for specific interface id
     * Called by EXPORT_MW_COM_INTERFACE macro
     * @param interface_id id of the interface used as key in interface operation map
     * @param ops pointer to IInterfaceOperations implementation
     */
    static void RegisterInterfaceOperation(const std::string_view& interface_id,
                                           std::shared_ptr<IInterfaceOperations> ops)
    {
        GetInterfaceFactories()[interface_id] = ops;
    }

    /**
     * Get interface operation for specific interface id
     * @param interface_id id of the interface used as key in interface operation map
     * @return It returns pointer to IInterfaceOperations implementation if found, nullptr otherwise
     */
    static IInterfaceOperations* GetInterfaceOperation(const std::string_view& interface_id)
    {
        auto it = GetInterfaceFactories().find(interface_id);
        if (it != GetInterfaceFactories().end())
        {
            return it->second.get();
        }
        return nullptr;
    }

    /**
     * Get type operation for specific type name
     * @param type_name name of the type used as key in type operation map
     * @return It returns pointer to ITypeOperations implementation if found, nullptr otherwise
     */
    static ITypeOperations* FindTypeInformation(const std::string_view& type_name)
    {
        auto& type_ops_map = GetTypeOperationMap();
        auto it = type_ops_map.find(type_name);
        if (it != type_ops_map.end())
        {
            return it->second.get();
        }
        return nullptr;
    }

    /**
     * Get member operation for specific interface id and member name
     * @param interface_id id of the interface used to find the interface operation
     * @param member_name name of the member used as key in member operation map
     * @return It returns pointer to IMemberOperation implementation if found, nullptr otherwise
     */
    static IMemberOperation* FindMemberOperation(const std::string_view& interface_id,
                                                 const std::string_view& member_name)
    {
        auto* factory = GetInterfaceOperation(interface_id);
        if (factory)
        {
            return factory->GetMemberOperation(member_name);
        }
        return nullptr;
    }
    /**
     * Get interface operation for specific interface id
     * @param interface_id id of the interface used as key in interface operation map
     * @return It returns pointer to IInterfaceOperations implementation if found, nullptr otherwise
     */
    static IInterfaceOperations* FindInterfaceRegistry(const std::string_view& interface_id)
    {
        return GetInterfaceOperation(interface_id);
    }
};

/*
 * EXPORT_MW_COM_INTERFACE macro
 * Create registry for interface operations
 * It registers interface operations for specific interface id
 * It also creates type aliases for ProxyType and SkeletonType
 * Using Static struct to register interface operations at startup or before main()
 * It uses constructor of static struct to register interface operations
 * id means Interface TYPE ID which rust side uses to identify the interface
 * proxy_type means Proxy class type
 * skeleton_type means Skeleton class type
 * Example usage:
 * EXPORT_MW_COM_INTERFACE(VehicleInterface, VehicleProxy, VehicleSkeleton)
 */
#define EXPORT_MW_COM_INTERFACE(id, proxy_type, skeleton_type)                                                       \
    /* Local tag for this specific id */                                                                             \
    struct id##_idTag                                                                                                \
    {                                                                                                                \
    };                                                                                                               \
    /* Type aliases for event macros to use */                                                                       \
    using id##_ProxyType = proxy_type;                                                                               \
    using id##_SkeletonType = skeleton_type;                                                                         \
                                                                                                                     \
    /* Registration helper struct - constructor runs at startup */                                                   \
    struct id##_InterfaceRegistrationHelper                                                                          \
    {                                                                                                                \
        id##_InterfaceRegistrationHelper()                                                                           \
        {                                                                                                            \
            using ProxyType = proxy_type;                                                                            \
            using SkeletonType = skeleton_type;                                                                      \
                                                                                                                     \
            auto interface_event_ops =                                                                               \
                std::make_shared<::score::mw::com::impl::rust::InterfaceOperationImpl<ProxyType, SkeletonType>>();     \
                                                                                                                     \
            /* Register interface factory for FFI layer */                                                           \
            ::score::mw::com::impl::rust::GlobalRegistryMapping::RegisterInterfaceOperation(#id, interface_event_ops); \
        }                                                                                                            \
    };                                                                                                               \
                                                                                                                     \
    /* Force instantiation at startup */                                                                             \
    static id##_InterfaceRegistrationHelper id##_interface_reg_instance;

/*
 * EXPORT_MW_COM_EVENT macro
 * Create registry for event member operations
 * It registers member operations for specific interface id and event name
 * It uses Static struct to register member operations at startup or before main()
 * It uses constructor of static struct to register member operations
 * id means Interface TYPE ID which rust side uses to identify the interface
 * event_type means event data type
 * event_member means event member name in Proxy and Skeleton classes
 * Example usage:
 * EXPORT_MW_COM_EVENT(VehicleInterface, Tire, left_tire)
 */
#define EXPORT_MW_COM_EVENT(id, event_type, event_member)                                                        \
    struct id##_##event_member##_EventRegistrationHelper                                                         \
    {                                                                                                            \
        id##_##event_member##_EventRegistrationHelper()                                                          \
        {                                                                                                        \
            using ProxyType = id##_ProxyType;                                                                    \
            using SkeletonType = id##_SkeletonType;                                                              \
                                                                                                                 \
            /* Build the operations struct */                                                                    \
            auto event_info =                                                                                    \
                std::make_shared<::score::mw::com::impl::rust::MemberOprationImpl<ProxyType,                       \
                                                                                SkeletonType,                    \
                                                                                event_type,                      \
                                                                                &ProxyType::event_member,        \
                                                                                &SkeletonType::event_member>>(); \
                                                                                                                 \
            /* Register this event in the LOCAL interface registry (not global) */                               \
            ::score::mw::com::impl::rust::GlobalRegistryMapping::RegisterMemberOperation(                          \
                std::string_view(#id), std::string_view(#event_member), event_info);                             \
        }                                                                                                        \
    };                                                                                                           \
                                                                                                                 \
    /* Force instantiation at startup */                                                                         \
    static id##_##event_member##_EventRegistrationHelper id##_##event_member##_event_reg_instance;

/*
 * EXPORT_MW_COM_TYPE macro
 * Create registry for type operations
 * It registers type operations for specific type name
 * It uses Static struct to register type operations at startup or before main()
 * It uses constructor of static struct to register type operations
 * type_tag means type name tag used in macros
 * type means actual C++ type
 * Example usage:
 * EXPORT_MW_COM_TYPE(TireType, Tire)
 */
#define EXPORT_MW_COM_TYPE(type_tag, type)                                                                          \
    extern "C" {                                                                                                    \
    /* Declare the generic Rust FFI function (same for all types) */                                                \
    void mw_com_impl_call_dyn_ref_fnmut_sample(const ::score::mw::com::impl::rust::FatPtr* boxed_fnmut,               \
                                               void* sample_ptr);                                                   \
    }                                                                                                               \
    template <>                                                                                                     \
    class score::mw::com::impl::rust::RustRefMutCallable<void, ::score::mw::com::impl::SamplePtr<type>>                 \
    {                                                                                                               \
      public:                                                                                                       \
        static void invoke(::score::mw::com::impl::rust::FatPtr ptr_,                                                 \
                           ::score::mw::com::impl::SamplePtr<type> sample) noexcept                                   \
        {                                                                                                           \
            /* Wrap in placement-new and call the generic Rust function */                                          \
            alignas(                                                                                                \
                ::score::mw::com::impl::SamplePtr<type>) char storage[sizeof(::score::mw::com::impl::SamplePtr<type>)]; \
            auto* placement_sample = new (storage)::score::mw::com::impl::SamplePtr<type>(std::move(sample));         \
            mw_com_impl_call_dyn_ref_fnmut_sample(&ptr_, placement_sample);                                         \
        }                                                                                                           \
        static void dispose(::score::mw::com::impl::rust::FatPtr) noexcept {}                                         \
    };                                                                                                              \
                                                                                                                    \
    struct type_tag##_TypeRegistrationHelper                                                                        \
    {                                                                                                               \
        type_tag##_TypeRegistrationHelper()                                                                         \
        {                                                                                                           \
            auto type_ops = std::make_shared<::score::mw::com::impl::rust::TypeOperationImpl<type>>();                \
            ::score::mw::com::impl::rust::GlobalRegistryMapping::RegisterTypeOperation(#type_tag, type_ops);          \
        }                                                                                                           \
    };                                                                                                              \
                                                                                                                    \
    static type_tag##_TypeRegistrationHelper type_tag##_type_reg_instance;
}  // namespace score::mw::com::impl::rust

#endif  // SCORE_MW_COM_IMPL_RUST_GENERIC_BRIDGE_MACROS_H
