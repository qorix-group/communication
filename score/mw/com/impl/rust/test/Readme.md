# Proxy-Skeleton Bridge Integration Tests

This directory contains integration tests for the Rust implementation of the COM middleware's proxy-skeleton bridge pattern.

## Overview

The test suite (`proxy_skeleton_bridge_int_test.rs`) validates the complete lifecycle of service communication using the IPC (MW::COM APIs), including:
- Service offering (skeleton side)
- Service discovery and consumption (proxy side)
- Event publication and subscription
- Event handlers and callbacks

## Test Cases

### 1. `test_skeleton_offer_service_integration`
Tests the skeleton (service provider) side functionality:
- Initializes mw_com with configuration
- Creates an instance specifier for `xpad/cp60/MapApiLanesStamped`
- Offers a service continuously in a background thread
- Publishes events every second for 10 seconds
- Gracefully stops the service offering

**Duration:** 10 seconds

### 2. `test_proxy_subscribe_integration`
Tests the proxy (service consumer) side functionality:
- Initializes mw_com and creates instance specifier
- Discovers and finds the offered service (with retry logic)
- Creates a proxy from the service handle
- Subscribes to events with a max sample count of 10
- Receives and processes samples for 5 seconds
- Unsubscribes and performs cleanup

**Duration:** 5 seconds (after service discovery)

### 3. `test_proxy_subscribe_with_handler_integration`
Tests event handler callback functionality:
- Subscribes to events from the service
- Sets up a receive handler callback
- Validates handler is triggered when new data arrives
- Unsets the handler and verifies no further callbacks
- Performs proper cleanup and unsubscription

**Duration:** ~4 seconds (3 seconds active + 1 second verification)

### 4. `test_multiple_proxies_subscribe_integration`
Tests multiple proxy clients subscribing to the same skeleton service:
- Created 3 concurrent proxy threads
- Each proxy independently discovers and connects to the same service
- All proxies subscribe to events with a max sample count of 10
- Each proxy receives and processes samples for 5 seconds
- Validates that multiple consumers can simultaneously receive events from one publisher
- Each proxy performs independent cleanup and unsubscription

**Duration:** 5 seconds (after service discovery) per proxy thread, running in parallel

## Running the Tests

Execute the integration tests with the following command:

```bash
bazel test --config=spp_host_gcc --test_output=streamed --nocache_test_results --test_arg=--nocapture //score/mw/com/impl/rust/test:proxy_bridge_integration_test
```

### Command Options Explained:
- `--config=spp_host_gcc`: Uses the SPP host GCC configuration
- `--test_output=streamed`: Streams test output in real-time
- `--nocache_test_results`: Forces fresh test execution
- `--test_arg=--nocapture`: Passes through print statements from tests

## Configuration

The tests require a configuration file located at:
```
score/mw/com/impl/rust/test/etc/mw_com_config.json
```

Make sure this file exists and has appropriate permissions before running the tests.

## Dependencies

- `proxy_bridge_rs`: Rust proxy bridge implementation
- `ipc_bridge_gen_rs`: Generated IPC bridge code for MapApiLanesStamped service

## Notes

- Tests use multi-threading to simulate concurrent skeleton and proxy operations
- Service discovery includes retry logic (up to 10 seconds) to handle timing issues
- All tests include proper cleanup and resource deallocation
- Tests are marked with `tags = ["manual"]` in the BUILD file for controlled execution
