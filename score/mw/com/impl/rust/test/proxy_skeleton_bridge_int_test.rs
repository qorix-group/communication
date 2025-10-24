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

/// Integration test that offers a service and keeps it running.
///
/// This test demonstrates the complete service lifecycle:
/// 1. Creating an instance specifier
/// 2. Offering a service continuously in a background thread
/// 3. Making the service discoverable for proxy tests
///
/// The service runs for 10 seconds, allowing proxy tests to connect and consume events.
/// It demonstrates the skeleton side (service offering) working alongside proxy consumers.
#[test]
fn test_skeleton_offer_service_integration() {
    use std::thread::{self, sleep};
    use std::time::Duration;

    println!("=== Starting Skeleton Service Offering Integration Test ===");

    // Spawn a background thread to run the skeleton service
    let skeleton_thread = thread::spawn(|| {
        println!("[Skeleton Thread] Initializing mw_com...");

        // Initialize mw_com with the config file
        let config_path = std::path::Path::new("score/mw/com/impl/rust/test/etc/mw_com_config.json");
        proxy_bridge_rs::initialize(Some(config_path));

        println!("[Skeleton Thread] Creating instance specifier...");
        // Create instance specifier
        let instance_specifier = proxy_bridge_rs::InstanceSpecifier::try_from("xpad/cp60/MapApiLanesStamped")
            .expect("Instance specifier creation failed");

        println!("[Skeleton Thread] Creating skeleton...");
        let skeleton = ipc_bridge_gen_rs::IpcBridge::Skeleton::new(&instance_specifier)
            .expect("BigDataSkeleton creation failed");

        println!("[Skeleton Thread] Offering service...");
        let skeleton = skeleton.offer_service().expect("Failed offering from rust");

        // Publish events for 10 seconds
        let start_time = std::time::Instant::now();
        let mut x: u32 = 0;

        while start_time.elapsed() < Duration::from_secs(10) {
            let mut sample: ipc_bridge_gen_rs::MapApiLanesStamped =
                ipc_bridge_gen_rs::MapApiLanesStamped::default();
            sample.x = x;

            match skeleton.events.map_api_lanes_stamped_.send(sample) {
                Ok(_) => {
                    println!("[Skeleton Thread] Published event {} (elapsed: {:?})", x, start_time.elapsed());
                    x += 1;
                }
                Err(e) => {
                    eprintln!("[Skeleton Thread] Failed to send event: {:?}", e);
                }
            }

            sleep(Duration::from_millis(1000));
        }

        println!("[Skeleton Thread] Service offering complete. Stopping service...");
        let _skeleton = skeleton.stop_offer_service();
        println!("[Skeleton Thread] Service stopped.");
    });

    // Wait for the service thread to complete
    println!("[Skeleton] Waiting for skeleton service thread to complete...");
    skeleton_thread.join().expect("Service thread panicked");
    println!("[Skeleton] Test complete!");
}



/// Integration test for proxy subscription to events
///
/// This test demonstrates:
/// 1. Finding a service using instance specifier
/// 2. Creating a proxy from the service handle
/// 3. Subscribing to an event
/// 4. Receiving samples from the event
/// 5. Unsubscribing and cleanup
#[test]
fn test_proxy_subscribe_integration() {
    use std::thread::sleep;
    use std::time::Duration;
    use std::thread;

    let proxy_thread = thread::spawn(|| {
        println!("[Proxy Thread] Initializing mw_com...");
        let config_path = std::path::Path::new("score/mw/com/impl/rust/test/etc/mw_com_config.json");
        proxy_bridge_rs::initialize(Some(config_path));

        println!("[Proxy Thread] Creating instance specifier...");
        let instance_specifier = proxy_bridge_rs::InstanceSpecifier::try_from("xpad/cp60/MapApiLanesStamped")
            .expect("Instance specifier creation failed");
        // taking start time
        let start_time = std::time::Instant::now();
        // handle_container is Option<HandleContainer> type for
        // properly handle the case where no service is found
        let mut handle_container = None;
        // proxy thread check for service availability for 5 seconds in each 1 second cycle
        while start_time.elapsed() < Duration::from_secs(5) {
            println!("[Proxy Thread] Finding service...");
            let handle_temp = proxy_bridge_rs::find_service(instance_specifier.clone())
                .expect("Failed to find service");
            match handle_temp.len() {
                0 => {
                    println!("[Proxy Thread] No service instances found yet - retrying...");
                }
                _ => {
                    handle_container = Some(handle_temp);
                    break;
                }
            }

            sleep(Duration::from_millis(1000));
        }
        if handle_container.is_none() || handle_container.as_ref().unwrap().len() == 0 {
            println!("[Proxy Thread] No service instances found - skipping test");
            println!("Note: Make sure the skeleton service is running first!");
            return;
        }
        // Extracts the HandleContainer from Option<HandleContainer>
        let handle_container = handle_container.unwrap();
        println!("[Proxy Thread] Found {} service instance(s)", handle_container.len());
        let service_handle = &handle_container[0];

        println!("[Proxy Thread] Creating proxy...");
        let proxy = ipc_bridge_gen_rs::IpcBridge::Proxy::new(service_handle)
            .expect("Failed to create proxy");

        println!("[Proxy Thread] Subscribing to event with max sample count of 10...");
        match proxy.map_api_lanes_stamped_.subscribe(10) {
            Ok(subscribed_event) => {
                println!("[Proxy Thread] Successfully subscribed to event!");

                // Receive samples for a 5 seconds
                let start_time = std::time::Instant::now();
                let mut sample_count = 0;
                //checking for new samples every 100 ms
                while start_time.elapsed() < Duration::from_secs(5) {
                    if let Some(sample) = subscribed_event.get_new_sample() {
                        println!("[Proxy Thread] Received sample #{}: x = {}", sample_count, sample.x);
                        sample_count += 1;
                        // Sample is automatically dropped here, ending the borrow
                    }

                    sleep(Duration::from_millis(100));
                }

                println!("[Proxy Thread] Received {} samples total", sample_count);

                // Unsubscribe from the event
                println!("[Proxy Thread] Unsubscribing from event...");
                let _unsubscribed_event = subscribed_event.unsubscribe();
                println!("[Proxy Test] Successfully unsubscribed!");
            }
            Err((error_msg, _event)) => {
                eprintln!("[Proxy Thread] Failed to subscribe: {}", error_msg);
                panic!("Subscription failed");
            }
        }
    });
    println!("[Proxy] Waiting for proxy service thread to complete...");
    proxy_thread.join().expect("Service thread panicked");
    println!("[Proxy] Test complete!");
}

/// Integration test for proxy subscription with receive handler
///
/// This test demonstrates:
/// 1. Subscribing to an event
/// 2. Setting a receive handler callback
/// 3. Handler being triggered when new data arrives
/// 4. Unsetting the handler
#[test]
fn test_proxy_subscribe_with_handler_integration() {
    use std::sync::{Arc, Mutex};
    use std::thread::sleep;
    use std::time::Duration;
    use std::thread;

    let proxy_thread = thread::spawn(|| {
        println!("[Handler Thread] Initializing mw_com...");
        let config_path = std::path::Path::new("score/mw/com/impl/rust/test/etc/mw_com_config.json");
        proxy_bridge_rs::initialize(Some(config_path));

        println!("[Handler Thread] Creating instance specifier...");
        let instance_specifier = proxy_bridge_rs::InstanceSpecifier::try_from("xpad/cp60/MapApiLanesStamped")
            .expect("Instance specifier creation failed");
        // taking start time
        let start_time = std::time::Instant::now();
        // handle_container is Option<HandleContainer> type for
        // properly handle the case where no service is found
        let mut handle_container = None;
        // proxy thread check for service availability for 5 seconds in each 1 second cycle
        while start_time.elapsed() < Duration::from_secs(5) {
            println!("[Handler Thread] Finding service...");
            let handle_temp = proxy_bridge_rs::find_service(instance_specifier.clone())
                .expect("Failed to find service");
            match handle_temp.len() {
                0 => {
                    println!("[Handler Thread] No service instances found yet - retrying...");
                }
                _ => {
                    handle_container = Some(handle_temp);
                    break;
                }
            }

            sleep(Duration::from_millis(1000));
        }
        if handle_container.is_none() || handle_container.as_ref().unwrap().len() == 0 {
            println!("[Handler Thread] No service instances found - skipping test");
            println!("Note: Make sure the skeleton service is running first!");
            return;
        }
        // Extracts the HandleContainer from Option<HandleContainer>
        let handle_container = handle_container.unwrap();
        // taking service handler
        let service_handle = &handle_container[0];

        println!("[Handler Thread] Creating proxy...");
        let proxy = ipc_bridge_gen_rs::IpcBridge::Proxy::new(service_handle)
            .expect("Failed to create proxy");

        println!("[Handler Thread] Subscribing to event...");
        // Subscribes to events with a max sample count of 10
        match proxy.map_api_lanes_stamped_.subscribe(10) {
            Ok(mut subscribed_event) => {
                println!("[Handler Thread] Successfully subscribed!");

                // Create shared counter wrapped in Arc<Mutex> for thread-safe access
                let handler_count = Arc::new(Mutex::new(0 as u32));
                let handler_count_clone = handler_count.clone();

                println!("[Handler Thread] Setting receive handler...");
                // Handler increments counter each time it's triggered
                subscribed_event.set_receive_handler(move || {
                    let mut count = handler_count_clone.lock().unwrap();
                    *count += 1;
                    println!("[Handler Thread] Receive handler called! (count: {})", *count);
                });

                println!("[Handler Thread] Waiting for events with handler active...");
                sleep(Duration::from_secs(3));

                let final_count = *handler_count.lock().unwrap();
                println!("[Handler Thread] Handler was called {} times", final_count);

                println!("[Handler Thread] Unsetting receive handler...");
                subscribed_event.unset_receive_handler();
                println!("[Handler Thread] Handler unset successfully");

                // Wait a bit more to ensure no more handler calls
                sleep(Duration::from_secs(1));

                let count_after_unset = *handler_count.lock().unwrap();
                // After unsetting, verify count doesn't increase
                assert_eq!(
                    final_count, count_after_unset,
                    "Handler should not be called after being unset"
                );

                println!("[Handler Thread] Unsubscribing...");
                let _unsubscribed_event = subscribed_event.unsubscribe();
                println!("[Handler Thread] Test complete!");
            }
            Err((error_msg, _event)) => {
                eprintln!("[Handler Thread] Failed to subscribe: {}", error_msg);
                panic!("Subscription failed");
            }
        }
    });
    println!("[Handler] Waiting for handler service thread to complete...");
    proxy_thread.join().expect("Service thread panicked");
    println!("[Handler] Test complete!");
}

/// Integration test for multiple proxies subscribing to the same skeleton
///
/// This test demonstrates:
/// 1. Multiple proxies connecting to the same service
/// 2. All proxies receiving the same events
/// 3. Independent subscription management by each proxy
#[test]
fn test_multiple_proxies_subscribe_integration() {
    use std::thread::{self, sleep};
    use std::time::Duration;

    let mut proxy_threads = vec![];

    for proxy_id in 0..3{
        let proxy_thread = thread::spawn(|| {
        println!("[Proxy Thread] Initializing mw_com...");
        let config_path = std::path::Path::new("score/mw/com/impl/rust/test/etc/mw_com_config.json");
        proxy_bridge_rs::initialize(Some(config_path));

        println!("[Proxy Thread] Creating instance specifier...");
        let instance_specifier = proxy_bridge_rs::InstanceSpecifier::try_from("xpad/cp60/MapApiLanesStamped")
            .expect("Instance specifier creation failed");
        // taking start time
        let start_time = std::time::Instant::now();
        // handle_container is Option<HandleContainer> type for
        // properly handle the case where no service is found
        let mut handle_container = None;
        // proxy thread check for service availability for 5 seconds in each 1 second cycle
        while start_time.elapsed() < Duration::from_secs(5) {
            println!("[Proxy Thread] Finding service...");
            let handle_temp = proxy_bridge_rs::find_service(instance_specifier.clone())
                .expect("Failed to find service");
            match handle_temp.len() {
                0 => {
                    println!("[Proxy Thread] No service instances found yet - retrying...");
                }
                _ => {
                    handle_container = Some(handle_temp);
                    break;
                }
            }

            sleep(Duration::from_millis(1000));
        }
        if handle_container.is_none() || handle_container.as_ref().unwrap().len() == 0 {
            println!("[Proxy Thread] No service instances found - skipping test");
            println!("Note: Make sure the skeleton service is running first!");
            return;
        }
        // Extracts the HandleContainer from Option<HandleContainer>
        let handle_container = handle_container.unwrap();
        println!("[Proxy Thread] Found {} service instance(s)", handle_container.len());
        let service_handle = &handle_container[0];

        println!("[Proxy Thread] Creating proxy...");
        let proxy = ipc_bridge_gen_rs::IpcBridge::Proxy::new(service_handle)
            .expect("Failed to create proxy");

        println!("[Proxy Thread] Subscribing to event with max sample count of 10...");
        match proxy.map_api_lanes_stamped_.subscribe(10) {
            Ok(subscribed_event) => {
                println!("[Proxy Thread] Successfully subscribed to event!");
                // Receive samples for a 5 seconds
                let start_time = std::time::Instant::now();
                let mut sample_count = 0;
                //checking for new samples every 100 ms
                while start_time.elapsed() < Duration::from_secs(5) {
                    if let Some(sample) = subscribed_event.get_new_sample() {
                        println!("[Proxy Thread] Received sample #{}: x = {}", sample_count, sample.x);
                        sample_count += 1;
                        // Sample is automatically dropped here, ending the borrow
                    }

                    sleep(Duration::from_millis(100));
                }

                println!("[Proxy Thread] Received {} samples total", sample_count);

                // Unsubscribe from the event
                println!("[Proxy Thread] Unsubscribing from event...");
                let _unsubscribed_event = subscribed_event.unsubscribe();
                println!("[Proxy Test] Successfully unsubscribed!");
            }
            Err((error_msg, _event)) => {
                eprintln!("[Proxy Thread] Failed to subscribe: {}", error_msg);
                panic!("Subscription failed");
            }
        }
    });
        proxy_threads.push(proxy_thread);
    }
    for thread_id in proxy_threads{
        println!("[Proxy] Waiting for Proxy ervice thread to complete...");
        thread_id.join().expect("Service thread panicked");
    }
    println!("[Proxy] Test complete!");
}
