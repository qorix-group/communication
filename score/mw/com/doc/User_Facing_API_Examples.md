# User Facing API Example

This document contains examples of each mw::com user facing API.

## `Error_Com` API Examples:

**Complete API Reference**: See [`score/mw/com/impl/com_error.h`](../impl/com_error.h) for all available ComErrc error codes.

### Example 1: Using `ComErrc` for InstanceIdentifier Creation Error Handling

`ComErrc` values are used to form the Error part of a `score::Result`.
For example, a function that creates an InstanceIdentifier could have this form:

<details>
<summary> Examples:</summary>

```cpp
score::Result<InstanceIdentifier> create(std::string_view serialized_format) {
    // other error handling
    if (does_not_contain_valid_InstanceIdentifier(serialized_format)) { // <- some helper function that performs a check
        // other error logging
        return MakeUnexpected(ComErrc::kInvalidInstanceIdentifierString);
    }
    // happy path of the code
    // return well formed InstanceIdentifier
}
```

**Key Points**:
- `ComErrc` values form the Error part of `score::Result<T>`
- `ComErrc::kInvalidInstanceIdentifierString`: Used for validation failures
- `MakeUnexpected(ComErrc)`: Converts error codes to Result error state
- Enables structured error handling without exceptions
</details>

---

### Example 2: Using `ComErrorDomain.MessageFor()` for Error Messages

`ComErrorDomain.MessageFor()` converts error codes to human-readable messages for logging and debugging.
It provides standardized error descriptions for all `ComErrc` error codes.

<details>
<summary> Examples:</summary>

``` cpp
auto error_message = ComErrorDomain.MessageFor(static_cast<score::result::ErrorCode>(score::mw::com::ComErrc::kServiceNotAvailable));
// error_message will contain: "Service is not available."
```
**Key Points**:
- `MessageFor()` converts error codes to human-readable messages
- Error codes must be cast to `score::result::ErrorCode` type
- Each error code has a predefined, standardized message
- Useful for logging and debugging
</details>

---

## `Runtime` API Examples:

### Example 1: Using `ResolveInstanceIDs()` for Instance Identifier Resolution

`ResolveInstanceIDs()` maps instance specifiers to instance identifier containers for service discovery.
It requires a valid `InstanceSpecifier` and validates shortname path format.

<details>
<summary> Examples:</summary>

```cpp
#include "score/mw/com/runtime.h"
#include "score/mw/com/impl/instance_specifier.h"

// Create a valid InstanceSpecifier from shortname path
auto instance_spec_result = score::mw::com::impl::InstanceSpecifier::Create("first/second/final");

if (instance_spec_result.has_value()) {
    auto instance_specifier = instance_spec_result.value();

    // Resolve instance IDs from the InstanceSpecifier
    auto result = score::mw::com::runtime::ResolveInstanceIDs(instance_specifier);

    if (result.has_value()) {
        auto instance_container = result.value();
        // Use the resolved instance identifiers
    } else {
        // Handle error - instance could not be resolved
        auto error = result.error();
    }
} else {
    // Handle error - invalid shortname path
    auto error = instance_spec_result.error();
}
```

**Key Points**:
- Requires `InstanceSpecifier` data type, not string literals
- `InstanceSpecifier::Create()` validates shortname path format (e.g., "first/second/final")
- `IsShortNameValid()` checks: alphanumeric/underscore/slash chars, no consecutive slashes, no trailing slash
- Returns `Result<InstanceIdentifierContainer>` - check with `has_value()`
- Requires runtime initialization before use
- Essential for service discovery scenarios
</details>

---

### Example 2: Using `InitializeRuntime()` for Runtime Setup

`InitializeRuntime()` sets up the mw::com system before any other mw::com functions can be used.
This function must be called once at the start of your application and supports multiple initialization methods.

#### Command Line Arguments Overload

This overload initializes the runtime using command line arguments for configuration.
It provides the simplest setup method when configuration comes from program startup parameters.

<details>
<summary> Examples:</summary>


```cpp
#include "score/mw/com/runtime.h"

int main(int argc, char* argv[]) {
    // Initialize mw::com runtime with command line arguments
    score::mw::com::runtime::InitializeRuntime(argc, argv);

    // Now you can use other mw::com functions
    // Runtime is initialized and ready to use

    return 0;
}
```

**Key Points**:
- Must be called once before using any mw::com functions
- Accepts `argc, argv` from main function for configuration
- Enables runtime initialization from command line arguments
- Simple one-line setup for basic usage
</details>

#### RuntimeConfiguration Overload

`InitializeRuntime()` can also be initialized with a `RuntimeConfiguration` object for direct configuration control.
This approach is ideal when you know the exact configuration file path at compile time.

<details>
<summary> Examples:</summary>

```cpp
#include "score/mw/com/runtime.h"
#include "score/mw/com/runtime_configuration.h"

int main() {
    // Create configuration with explicit path
    score::mw::com::runtime::RuntimeConfiguration config{"/path/to/mw_com_config.json"};

    // Initialize mw::com runtime with the configuration
    score::mw::com::runtime::InitializeRuntime(config);

    // Runtime is now ready to use
    return 0;
}
```

#### Key Points
- Creates `RuntimeConfiguration` with explicit config file path
- Direct initialization without command line arguments
- Ideal when configuration path is known at compile time
- Two-step process: create config object, then initialize runtime
</details>

---

### Example 3: Using `RuntimeConfiguration` for Configuration Management

`RuntimeConfiguration` provides flexible ways to manage mw::com configuration paths and supports multiple initialization methods.
It offers constructors for default paths, explicit paths, and command line argument parsing.

<details>
<summary> Examples:</summary>

```cpp
#include "score/mw/com/runtime_configuration.h"

// Default constructor - uses "./etc/mw_com_config.json"
score::mw::com::runtime::RuntimeConfiguration default_config;

// Explicit path constructor
score::mw::com::runtime::RuntimeConfiguration explicit_config{"/custom/path/config.json"};

// Command line arguments constructor
score::mw::com::runtime::RuntimeConfiguration cmd_config{argc, argv};

// Get the resolved configuration path
const auto& config_path = default_config.GetConfigurationPath();
```

#### Key Points
- Default constructor uses `"./etc/mw_com_config.json"` as fallback path
- Explicit path constructor allows direct configuration file specification
- Command line constructor accepts `argc, argv` for configuration parsing
- `GetConfigurationPath()` returns the resolved configuration file path

</details>

---
