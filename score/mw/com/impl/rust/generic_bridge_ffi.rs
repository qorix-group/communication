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

use std::ffi::c_char;

/// Opaque C++ void* pointer wrapper
pub type CVoidPtr = *const std::ffi::c_void;
pub type CMutVoidPtr = *mut std::ffi::c_void;

pub use proxy_bridge_rs::ffi::{FatPtr, HandleType, ProxyEventBase, ProxyWrapperClass};
pub use proxy_bridge_rs::NativeInstanceSpecifier;

/// Opaque proxy base struct
#[repr(C)]
pub struct ProxyBase {
    dummy: [u8; 0],
}

/// Opaque skeleton base struct
#[repr(C)]
pub struct SkeletonBase {
    dummy: [u8; 0],
}

/// Opaque skeleton event base struct
#[repr(C)]
pub struct SkeletonEventBase {
    dummy: [u8; 0],
}

/// Generic Rust closure invocation for all types
///
/// This function is called by C++ to invoke a Rust closure with a sample pointer.
/// The closure is reconstructed from the FatPtr trait object.
///
/// # Safety
/// - `ptr` must point to a valid FatPtr
/// - `sample_ptr` must point to valid placement-new storage containing SamplePtr<T>
///
#[no_mangle]
pub extern "C" fn mw_com_impl_call_dyn_ref_fnmut_sample(
    ptr: *const FatPtr,
    sample_ptr: *mut std::ffi::c_void,
) {
    if ptr.is_null() || sample_ptr.is_null() {
        return;
    }

    // Reconstruct the closure from FatPtr
    let callable: &mut dyn FnMut(*mut std::ffi::c_void) = unsafe { std::mem::transmute(*ptr) };

    // Invoke the closure with the void* sample pointer
    callable(sample_ptr);
}

// FFI declarations for C++ generic event bridge functions
#[allow(unsafe_code)]
extern "C" {

    /// Subscribe to event to start receiving samples
    ///
    /// # Arguments
    /// * `event_ptr` - Opaque event pointer
    /// * `max_sample_count` - Maximum number of samples to buffer
    /// # Returns
    /// true if subscription successful, false otherwise
    pub fn mw_com_proxy_event_subscribe(
        event_ptr: *mut ProxyEventBase,
        max_sample_count: u32,
    ) -> bool;

    /// Get event pointer from proxy by event name
    ///
    /// # Arguments
    /// * `proxy_ptr` - Opaque proxy pointer
    /// * `interface_id` - UTF-8 C string of interface UID
    /// * `event_id` - UTF-8 C string of event name
    ///
    /// # Returns
    /// Opaque event pointer, or nullptr if event not found
    pub fn mw_com_get_event_from_proxy(
        proxy_ptr: *mut ProxyBase,
        interface_id: *const c_char,
        event_id: *const c_char,
    ) -> *mut ProxyEventBase;

    /// Get event pointer from skeleton by event name
    ///
    /// # Arguments
    /// * `skeleton_ptr` - Opaque skeleton pointer
    /// * `interface_id` - UTF-8 C string of interface UID
    /// * `event_id` - UTF-8 C string of event name
    ///
    /// # Returns
    /// Opaque event pointer, or nullptr if event not found
    pub fn mw_com_get_event_from_skeleton(
        skeleton_ptr: *mut SkeletonBase,
        interface_id: *const c_char,
        event_id: *const c_char,
    ) -> *mut SkeletonEventBase;

    /// Get new samples from an event
    ///
    /// # Arguments
    /// * `event_ptr` - Opaque event pointer
    /// * `event_id` - UTF-8 C string of event name
    /// * `callback` - FatPtr to callback function
    /// * `max_samples` - Maximum number of samples to retrieve
    ///
    /// # Returns
    /// Number of samples retrieved
    pub fn mw_com_type_registry_get_samples_from_event(
        event_ptr: *mut ProxyEventBase,
        event_type: *const c_char,
        callback: *const FatPtr,
        max_samples: u32,
    ) -> u32;

    /// Send data via skeleton event
    ///
    /// # Arguments
    /// * `event_ptr` - Opaque skeleton event pointer
    /// * `event_id` - UTF-8 C string of event name
    /// * `data_ptr` - Pointer to event data
    ///
    /// # Returns
    /// true if send successful, false otherwise
    pub fn mw_com_skeleton_send_event(
        event_ptr: *mut SkeletonEventBase,
        event_type: *const c_char,
        data_ptr: CVoidPtr,
    );

    /// Create proxy by UID and handle
    ///
    /// # Arguments
    /// * `interface_id` - UTF-8 C string of interface UID
    /// * `handle_ptr` - Opaque handle pointer
    ///
    /// # Returns
    /// Opaque proxy pointer, or nullptr if creation failed
    pub fn mw_com_create_proxy(
        interface_id: *const c_char,
        handle_ptr: &HandleType,
    ) -> *mut ProxyBase;

    /// Create skeleton by UID and instance specifier
    ///
    /// # Arguments
    /// * `interface_id` - UTF-8 C string of interface UID
    /// * `instance_spec` - Opaque instance specifier pointer
    ///
    /// # Returns
    /// Opaque skeleton pointer, or nullptr if creation failed
    pub fn mw_com_create_skeleton(
        interface_id: *const c_char,
        instance_spec: NativeInstanceSpecifier,
    ) -> *mut SkeletonBase;

    /// Destroy proxy
    ///
    /// # Arguments
    /// * `interface_id` - UTF-8 C string of interface UID
    /// * `proxy_ptr` - Opaque proxy pointer
    pub fn mw_com_destroy_proxy(interface_id: *const c_char, proxy_ptr: *mut ProxyBase);

    /// Destroy skeleton
    ///
    /// # Arguments
    /// * `interface_id` - UTF-8 C string of interface UID
    /// * `skeleton_ptr` - Opaque skeleton pointer
    pub fn mw_com_destroy_skeleton(interface_id: *const c_char, skeleton_ptr: *mut SkeletonBase);

    /// Offer service via skeleton
    ///
    /// # Arguments
    /// * `interface_id` - UTF-8 C string of interface UID
    /// * `skeleton_ptr` - Opaque skeleton pointer
    ///
    /// # Returns
    /// true if offer successful, false otherwise
    pub fn mw_com_skeleton_offer_service(
        interface_id: *const c_char,
        skeleton_ptr: *mut SkeletonBase,
    ) -> bool;

    /// Stop offering service via skeleton
    ///
    /// # Arguments
    /// * `interface_id` - UTF-8 C string of interface UID
    /// * `skeleton_ptr` - Opaque skeleton pointer
    pub fn mw_com_skeleton_stop_offer_service(
        interface_id: *const c_char,
        skeleton_ptr: *mut SkeletonBase,
    );
}

/// Safe wrapper around mw_com_skeleton_offer_service
///
/// # Arguments
/// * `interface_id` - Interface UID string
/// * `skeleton_ptr` - Opaque skeleton pointer
///
/// # Returns
/// true if offer was successful
#[allow(unsafe_code)]
pub fn skeleton_offer_service(
    interface_id: &str,
    skeleton_ptr: *mut SkeletonBase,
) -> Result<bool, std::ffi::NulError> {
    let c_uid = std::ffi::CString::new(interface_id)?;
    let status = unsafe { mw_com_skeleton_offer_service(c_uid.as_ptr(), skeleton_ptr) };
    Ok(status)
}

/// Safe wrapper around mw_com_skeleton_stop_offer_service
///
/// # Arguments
/// * `interface_id` - Interface UID string
/// * `skeleton_ptr` - Opaque skeleton pointer
///
/// # Returns
/// Result enum
#[allow(unsafe_code)]
pub fn skeleton_stop_offer_service(
    interface_id: &str,
    skeleton_ptr: *mut SkeletonBase,
) -> Result<(), std::ffi::NulError> {
    let c_uid = std::ffi::CString::new(interface_id)?;
    unsafe { mw_com_skeleton_stop_offer_service(c_uid.as_ptr(), skeleton_ptr) };
    Ok(())
}

/// Safe wrapper around mw_com_create_proxy
///
/// # Arguments
/// * `interface_id` - Interface UID string
/// * `handle_ptr` - Opaque handle pointer
///
/// # Returns
/// Opaque proxy pointer, or error if creation failed
#[allow(unsafe_code)]
pub fn create_proxy(
    interface_id: &str,
    handle_ptr: &HandleType,
) -> Result<*mut ProxyBase, std::ffi::NulError> {
    let c_uid = std::ffi::CString::new(interface_id)?;
    Ok(unsafe { mw_com_create_proxy(c_uid.as_ptr(), &handle_ptr) })
}

/// Safe wrapper around mw_com_create_skeleton
///
/// # Arguments
/// * `interface_id` - Interface UID string
/// * `instance_spec` - Opaque instance specifier pointer
///
/// # Returns
/// Opaque skeleton pointer, or error if creation failed
#[allow(unsafe_code)]
pub fn create_skeleton(
    interface_id: &str,
    instance_spec: NativeInstanceSpecifier,
) -> Result<*mut SkeletonBase, std::ffi::NulError> {
    let c_uid = std::ffi::CString::new(interface_id)?;
    Ok(unsafe { mw_com_create_skeleton(c_uid.as_ptr(), instance_spec) })
}

/// Safe wrapper around mw_com_destroy_proxy
///
/// # Arguments
/// * `interface_id` - Interface UID string
/// * `proxy_ptr` - Opaque proxy pointer to destroy
///
/// # Returns
/// Error if interface_id contains null bytes
#[allow(unsafe_code)]
pub fn destroy_proxy(
    interface_id: &str,
    proxy_ptr: *mut ProxyBase,
) -> Result<(), std::ffi::NulError> {
    let c_uid = std::ffi::CString::new(interface_id)?;
    unsafe { mw_com_destroy_proxy(c_uid.as_ptr(), proxy_ptr) };
    Ok(())
}

/// Safe wrapper around mw_com_destroy_skeleton
///
/// # Arguments
/// * `interface_id` - Interface UID string
/// * `skeleton_ptr` - Opaque skeleton pointer to destroy
///
/// # Returns
/// Error if interface_id contains null bytes
#[allow(unsafe_code)]
pub fn destroy_skeleton(
    interface_id: &str,
    skeleton_ptr: *mut SkeletonBase,
) -> Result<(), std::ffi::NulError> {
    let c_uid = std::ffi::CString::new(interface_id)?;
    unsafe { mw_com_destroy_skeleton(c_uid.as_ptr(), skeleton_ptr) };
    Ok(())
}

/// Safe wrapper around mw_com_get_event_from_proxy
///
/// # Arguments
/// * `proxy_ptr` - Opaque proxy pointer
/// * `interface_id` - Interface UID string
/// * `event_id` - Event name string
///
/// # Returns
/// Opaque event pointer, or error if not found
#[allow(unsafe_code)]
pub fn get_event_from_proxy(
    proxy_ptr: *mut ProxyBase,
    interface_id: &str,
    event_id: &str,
) -> Result<*mut ProxyEventBase, std::ffi::NulError> {
    let c_id = std::ffi::CString::new(interface_id)?;
    let c_name = std::ffi::CString::new(event_id)?;
    Ok(unsafe { mw_com_get_event_from_proxy(proxy_ptr, c_id.as_ptr(), c_name.as_ptr()) })
}

/// Safe wrapper around mw_com_get_event_from_skeleton
///
/// # Arguments
/// * `skeleton_ptr` - Opaque skeleton pointer
/// * `interface_id` - Interface UID string
/// * `event_id` - Event name string
///
/// # Returns
/// Opaque event pointer, or error if not found
#[allow(unsafe_code)]
pub fn get_event_from_skeleton(
    skeleton_ptr: *mut SkeletonBase,
    interface_id: &str,
    event_id: &str,
) -> Result<*mut SkeletonEventBase, std::ffi::NulError> {
    let c_id = std::ffi::CString::new(interface_id)?;
    let c_name = std::ffi::CString::new(event_id)?;
    Ok(unsafe { mw_com_get_event_from_skeleton(skeleton_ptr, c_id.as_ptr(), c_name.as_ptr()) })
}

/// Safe wrapper around mw_com_get_samples_from_event
///
/// # Arguments
/// * `event_ptr` - Opaque event pointer
/// * `event_id` - Event name string
/// * `callback` - FatPtr to callback function
/// * `max_samples` - Maximum number of samples
///
/// # Returns
/// Number of samples retrieved
#[allow(unsafe_code)]
pub fn get_samples_from_event(
    event_ptr: *mut ProxyEventBase,
    event_type: &str,
    callback: &FatPtr,
    max_samples: u32,
) -> Result<u32, std::ffi::NulError> {
    let c_name = std::ffi::CString::new(event_type)?;
    Ok(unsafe {
        mw_com_type_registry_get_samples_from_event(
            event_ptr,
            c_name.as_ptr(),
            callback,
            max_samples,
        )
    })
}

/// Safe wrapper around mw_com_skeleton_send_event
///
/// # Arguments
/// * `event_ptr` - Opaque skeleton event pointer
/// * `event_id` - Event name string
/// * `data_ptr` - Pointer to event data
#[allow(unsafe_code)]
pub fn skeleton_send_event(
    event_ptr: *mut SkeletonEventBase,
    event_type: &str,
    data_ptr: *const std::ffi::c_void,
) {
    let c_name = std::ffi::CString::new(event_type).expect("CString::new failed");
    unsafe { mw_com_skeleton_send_event(event_ptr, c_name.as_ptr(), data_ptr) };
}

/// Safe wrapper around mw_com_proxy_event_subscribe
///
/// # Arguments
/// * `event_ptr` - Opaque event pointer
/// * `max_sample_count` - Maximum number of samples to buffer
///
/// # Returns
/// true if subscription was successful
#[allow(unsafe_code)]
pub fn subscribe_to_event(event_ptr: *mut ProxyEventBase, max_sample_count: u32) -> bool {
    unsafe { mw_com_proxy_event_subscribe(event_ptr, max_sample_count) }
}
