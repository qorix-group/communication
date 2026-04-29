# IPC Bridge Communication Examples

This directory contains two complementary examples demonstrating different communication patterns in SCORE middleware:

## 1. Event-based Communication (Asynchronous Publish-Subscribe)

**Files:**
- `sample_sender_receiver.h/cpp` - Event sender/receiver implementation
- `main.cpp` - Entry point for event example
- `datatype.h` - Data types including `MapApiLanesStamped` event

**Key Characteristics:**
- **Asynchronous**: Publisher sends events; subscribers are notified when data arrives
- **One-to-Many**: Multiple consumers can subscribe to the same event
- **Loose Coupling**: Publisher and subscribers don't need to directly interact
- **Pattern**: Publish-Subscribe

**Usage Pattern:**
```cpp
// Provider (Skeleton)
IpcBridgeSkeleton skeleton = IpcBridgeSkeleton::Create(instance_specifier).value();
skeleton.OfferService();
skeleton.map_api_lanes_stamped_.Send(sample);

// Consumer (Proxy)
IpcBridgeProxy proxy = IpcBridgeProxy::Create(handle).value();
proxy.map_api_lanes_stamped_.Subscribe(SAMPLES_PER_CYCLE);
proxy.map_api_lanes_stamped_.GetNewSamples([](SamplePtr<MapApiLanesStamped> sample) {
    // Process event
});
```

**Running the Event Example:**
```bash
# Terminal 1 - Start receiver (consumer)
./example --mode recv --cycle-time 100 --num-cycles 100 --service_instance_manifest config.json

# Terminal 2 - Start sender (provider) 
./example --mode send --cycle-time 100 --num-cycles 100 --service_instance_manifest config.json
```

---

## 2. Method-based Communication (Synchronous Request-Response)

**Files:**
- `sample_method_caller.h/cpp` - Method call implementation
- `main_method.cpp` - Entry point for method example
- `datatype.h` (extended) - New data types including `RouteInformation` method data

**Key Characteristics:**
- **Synchronous**: Caller waits for the method to complete and receive a response
- **Request-Response**: Each call has a corresponding result
- **Tightly Coupled**: Consumer must know the provider is available
- **Pattern**: Remote Procedure Call (RPC)

**Usage Pattern:**
```cpp
// Provider (Skeleton)
IpcBridgeSkeleton skeleton = IpcBridgeSkeleton::Create(instance_specifier).value();
skeleton.OfferService();

skeleton.calculate_route_hash.RegisterHandler([](std::uint32_t segment_id) -> std::uint64_t {
    // Calculate and return hash
    return computed_hash;
});

// Consumer (Proxy)
IpcBridgeProxy proxy = IpcBridgeProxy::Create(handle).value();
auto result = proxy.calculate_route_hash(segment_id);
std::uint64_t hash = *result.value();
```

**Running the Method Example:**
```bash
# Terminal 1 - Start provider
./method_example --mode provider --cycle-time 100 --num-cycles 0 --service_instance_manifest config.json

# Terminal 2 - Start consumer
./method_example --mode consumer --cycle-time 100 --num-cycles 10 --service_instance_manifest config.json
```

---

## Comparison: Events vs Methods

| Aspect | Events | Methods |
|--------|--------|---------|
| **Calling Pattern** | Asynchronous | Synchronous |
| **Cardinality** | One-to-Many | One-to-One (per call) |
| **Data Flow** | Publisher → Subscribers | Consumer ← → Provider |
| **Coupling** | Loose | Tight |
| **Response** | No direct response | Direct return value |
| **Use Case** | Notifications, streams | RPC calls, immediate response |
| **Latency Tolerance** | More flexible | Requires quick response |
| **Example File** | `sample_sender_receiver.*` | `sample_method_caller.*` |

---

## Data Types

### Event Data
- `MapApiLanesStamped`: Contains HD map lane data with timestamp and quality information

### Method Data  
- `RouteSegmentInfo`: Single segment information for route calculation
- `RouteInformation`: Complete route with multiple segments
- Methods use built-in types (e.g., `std::uint32_t`, `std::uint64_t`) for simple parameters and returns

---

## Implementation Details

### Event Implementation
The event example (`sample_sender_receiver.*`) demonstrates:
1. **Skeleton side**: 
   - Creating service instances
   - Offering services
   - Sending samples at regular cycles
   - Allocating shared memory samples

2. **Proxy side**:
   - Finding services
   - Creating proxies
   - Subscribing to events
   - Setting event handlers (optional callback-based mode)
   - Polling for new samples with `GetNewSamples()`

### Method Implementation
The method example (`sample_method_caller.*`) demonstrates:
1. **Skeleton side**:
   - Creating service instances
   - Offering services
   - Registering method handlers with lambda functions
   - Accessing method input parameters and returning results

2. **Proxy side**:
   - Finding services
   - Creating proxies
   - Calling methods directly with arguments
   - Receiving return values synchronously
   - Error handling for method calls

---

## Building the Examples

Both examples are built using Bazel. Update the `BUILD` file in the `ipc_bridge` directory to include:

```python
# Event example binary
cc_binary(
    name = "event_example",
    srcs = [
        "main.cpp",
        "sample_sender_receiver.cpp",
    ],
    deps = [
        # dependencies
    ],
)

# Method example binary
cc_binary(
    name = "method_example",
    srcs = [
        "main_method.cpp",
        "sample_method_caller.cpp",
    ],
    deps = [
        # dependencies
    ],
)
```

---

## Key Differences in Data Flow

### Events (Pub-Sub):
```
Cycle 1: [Skeleton] Send Sample 1 → [Middleware] → [Proxy] Receive Sample 1
Cycle 2: [Skeleton] Send Sample 2 → [Middleware] → [Proxy] Receive Sample 2
```

### Methods (RPC):
```
[Proxy] Call method(arg1, arg2) →
  [Middleware] Route request →
    [Skeleton] Execute handler(arg1, arg2) →
      [Middleware] Return result →
        [Proxy] Receive result synchronously
```

---

## Notes

- Both examples use template-based interfaces defined in `datatype.h`
- The `IpcBridgeInterface` now contains both events and methods
- Both proxy and skeleton implementations support zero-copy and copy-based communication patterns
- The examples include hash calculation for data verification (events) and method computation (methods)
- Error handling is demonstrated for both communication patterns

For detailed API information, refer to the SCORE middleware COM module documentation.
