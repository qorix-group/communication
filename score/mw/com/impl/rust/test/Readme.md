# Proxy-Skeleton Bridge Integration Tests

This directory contains integration tests for the Rust implementation of the COM middleware's proxy-skeleton bridge pattern.

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
- Service discovery includes retry logic (up to 2 seconds) to handle timing issues
- All tests include proper cleanup and resource deallocation
- Tests are marked with `tags = ["manual"]` in the BUILD file for controlled execution
