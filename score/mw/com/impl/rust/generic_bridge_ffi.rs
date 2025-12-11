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

pub use proxy_bridge_rs::FatPtr;
pub use proxy_bridge_rs::HandleType;
pub use proxy_bridge_rs::ProxyEventBase;
pub use proxy_bridge_rs::ProxyWrapperClass;

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

/// String view similar to C++'s std::string_view
/// Holds a pointer to a string and its length without requiring null termination
#[repr(C)]
pub struct StringView {
    data: *const c_char,
    len: u32,
}

impl StringView {
    /// Create a StringView from a Rust &str
    pub fn from_str(s: &str) -> Self {
        if s.is_empty() {
            panic!("StringView: empty string is not allowed");
        }
        StringView {
            data: s.as_ptr() as *const c_char,
            len: s.len() as u32,
        }
    }
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
    // SAFETY: ptr is guaranteed to point to a valid FatPtr that was created by the C++ side
    // and represents a valid dynamic trait object (dyn FnMut). The transmute is safe because
    // FatPtr is a binary representation of a trait object's fat pointer (data + vtable).
    // The caller must ensure the lifetime of the closure outlives this call.
    let callable: &mut dyn FnMut(*mut std::ffi::c_void) = unsafe { std::mem::transmute(*ptr) };

    // Invoke the closure with the void* sample pointer
    callable(sample_ptr);
}

// FFI declarations for C++ generic event bridge functions
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
        interface_id: StringView,
        event_id: StringView,
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
        interface_id: StringView,
        event_id: StringView,
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
        event_type: StringView,
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
        event_type: StringView,
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
        interface_id: StringView,
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
        interface_id: StringView,
        instance_spec: NativeInstanceSpecifier,
    ) -> *mut SkeletonBase;

    /// Destroy proxy
    ///
    /// # Arguments
    /// * `proxy_ptr` - Opaque proxy pointer
    pub fn mw_com_destroy_proxy(proxy_ptr: *mut ProxyBase);

    /// Destroy skeleton
    ///
    /// # Arguments
    /// * `skeleton_ptr` - Opaque skeleton pointer
    pub fn mw_com_destroy_skeleton(skeleton_ptr: *mut SkeletonBase);

    /// Offer service via skeleton
    ///
    /// # Arguments
    /// * `skeleton_ptr` - Opaque skeleton pointer
    ///
    /// # Returns
    /// true if offer successful, false otherwise
    pub fn mw_com_skeleton_offer_service(
        skeleton_ptr: *mut SkeletonBase,
    ) -> bool;

    /// Stop offering service via skeleton
    ///
    /// # Arguments
    /// * `skeleton_ptr` - Opaque skeleton pointer
    pub fn mw_com_skeleton_stop_offer_service(
        skeleton_ptr: *mut SkeletonBase,
    );
}

/// Safe wrapper around mw_com_skeleton_offer_service
///
/// # Arguments
/// * `skeleton_ptr` - Opaque skeleton pointer
///
/// # Returns
/// true if offer was successful
pub fn skeleton_offer_service(
    skeleton_ptr: *mut SkeletonBase,
) -> bool {
    // SAFETY: skeleton_ptr is guaranteed to be a valid pointer to a SkeletonBase that was
    // previously created via mw_com_create_skeleton. The C++ side guarantees the pointer
    // remains valid for the lifetime of the skeleton object.
    unsafe { mw_com_skeleton_offer_service(skeleton_ptr) }
}

/// Safe wrapper around mw_com_skeleton_stop_offer_service
///
/// # Arguments
/// * `skeleton_ptr` - Opaque skeleton pointer
///
/// # Returns
/// Result enum indicating success or error
pub fn skeleton_stop_offer_service(
    skeleton_ptr: *mut SkeletonBase,
) {
    // SAFETY: skeleton_ptr is guaranteed to be a valid pointer to a SkeletonBase that was
    // previously created via mw_com_create_skeleton and is still alive. The caller must ensure
    // the skeleton_ptr has not already been destroyed.
    unsafe { mw_com_skeleton_stop_offer_service(skeleton_ptr) };
}

/// Safe wrapper around mw_com_create_proxy
///
/// # Arguments
/// * `interface_id` - Interface UID string
/// * `handle_ptr` - Opaque handle pointer
///
/// # Returns
/// Opaque proxy pointer, or error if creation failed
pub fn create_proxy(
    interface_id: &str,
    handle_ptr: &HandleType,
) -> *mut ProxyBase {
    let c_uid = StringView::from_str(interface_id);
    // SAFETY: handle_ptr is a valid reference to a HandleType, so the pointer is valid.
    // The C++ implementation creates and returns a valid ProxyBase pointer that the caller
    // is responsible for managing and eventually destroying with destroy_proxy().
    unsafe { mw_com_create_proxy(c_uid, &handle_ptr) }
}

/// Safe wrapper around mw_com_create_skeleton
///
/// # Arguments
/// * `interface_id` - Interface UID string
/// * `instance_spec` - Opaque instance specifier pointer
///
/// # Returns
/// Opaque skeleton pointer, or error if creation failed
pub fn create_skeleton(
    interface_id: &str,
    instance_spec: NativeInstanceSpecifier,
) -> *mut SkeletonBase {
    let c_uid = StringView::from_str(interface_id);
    // SAFETY: instance_spec is a valid NativeInstanceSpecifier provided by the caller.
    // The C++ implementation creates and returns a valid SkeletonBase pointer that the caller
    // is responsible for managing and eventually destroying with destroy_skeleton().
    unsafe { mw_com_create_skeleton(c_uid, instance_spec) }
}

/// Safe wrapper around mw_com_destroy_proxy
///
/// # Arguments
/// * `interface_id` - Interface UID string
/// * `proxy_ptr` - Opaque proxy pointer to destroy
///
/// # Returns
/// Error if interface_id contains null bytes
pub fn destroy_proxy(
    proxy_ptr: *mut ProxyBase,
) {
    // SAFETY: proxy_ptr must be a valid pointer returned from create_proxy() that has not been
    // destroyed yet. The caller must ensure no other references to this proxy exist after
    // calling this function.
    unsafe { mw_com_destroy_proxy(proxy_ptr) };
}

/// Safe wrapper around mw_com_destroy_skeleton
///
/// # Arguments
/// * `interface_id` - Interface UID string
/// * `skeleton_ptr` - Opaque skeleton pointer to destroy
///
/// # Returns
/// Error if interface_id contains null bytes
pub fn destroy_skeleton(
    skeleton_ptr: *mut SkeletonBase,
) {
    // SAFETY: skeleton_ptr must be a valid pointer returned from create_skeleton() that has not
    // been destroyed yet. The caller must ensure no other references to this skeleton exist
    // after calling this function.
    unsafe { mw_com_destroy_skeleton(skeleton_ptr) };
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
pub fn get_event_from_proxy(
    proxy_ptr: *mut ProxyBase,
    interface_id: &str,
    event_id: &str,
) -> *mut ProxyEventBase {
    let c_id = StringView::from_str(interface_id);
    let c_name = StringView::from_str(event_id);
    // SAFETY: proxy_ptr is a valid pointer to a ProxyBase previously created with create_proxy().
    // The C++ implementation returns a valid ProxyEventBase pointer or nullptr if event not found.
    // The returned pointer remains valid as long as the proxy remains alive.
    unsafe { mw_com_get_event_from_proxy(proxy_ptr, c_id, c_name) }
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
pub fn get_event_from_skeleton(
    skeleton_ptr: *mut SkeletonBase,
    interface_id: &str,
    event_id: &str,
) -> *mut SkeletonEventBase{
    let c_id = StringView::from_str(interface_id);
    let c_name = StringView::from_str(event_id);
    // SAFETY: skeleton_ptr is a valid pointer to a SkeletonBase previously created with
    // create_skeleton(). The C++ implementation returns a valid SkeletonEventBase pointer or
    // nullptr if event not found. The returned pointer remains valid as long as the skeleton
    // remains alive.
    unsafe { mw_com_get_event_from_skeleton(skeleton_ptr, c_id, c_name) }
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
pub fn get_samples_from_event(
    event_ptr: *mut ProxyEventBase,
    event_type: &str,
    callback: &FatPtr,
    max_samples: u32,
) -> u32 {
    let c_name = StringView::from_str(event_type);
    // SAFETY: event_ptr is a valid pointer to a ProxyEventBase previously obtained from
    // get_event_from_proxy(). callback is a valid FatPtr to a callable that was created by
    // the C++ side. The C++ implementation handles the callback invocation safely.
    unsafe {
        mw_com_type_registry_get_samples_from_event(
            event_ptr,
            c_name,
            callback,
            max_samples,
        )
    }
}

/// Safe wrapper around mw_com_skeleton_send_event
///
/// # Arguments
/// * `event_ptr` - Opaque skeleton event pointer
/// * `event_id` - Event name string
/// * `data_ptr` - Pointer to event data
pub fn skeleton_send_event(
    event_ptr: *mut SkeletonEventBase,
    event_type: &str,
    data_ptr: *const std::ffi::c_void,
) {
    let c_name = StringView::from_str(event_type);
    // SAFETY: event_ptr is a valid pointer to a SkeletonEventBase previously obtained from
    // get_event_from_skeleton(). data_ptr is a pointer to the event data provided by the caller,
    // which must point to valid data matching the event type. The C++ implementation handles
    // the data sending safely.
    unsafe { mw_com_skeleton_send_event(event_ptr, c_name, data_ptr) };
}

/// Safe wrapper around mw_com_proxy_event_subscribe
///
/// # Arguments
/// * `event_ptr` - Opaque event pointer
/// * `max_sample_count` - Maximum number of samples to buffer
///
/// # Returns
/// true if subscription was successful
pub fn subscribe_to_event(event_ptr: *mut ProxyEventBase, max_sample_count: u32) -> bool {
    // SAFETY: event_ptr is a valid pointer to a ProxyEventBase previously obtained from
    // get_event_from_proxy(). The C++ implementation handles subscription safely and
    // returns a boolean indicating success or failure.
    unsafe { mw_com_proxy_event_subscribe(event_ptr, max_sample_count) }
}
