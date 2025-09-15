# User Facing API Example

This document contains examples of each mw::com user facing API.

## `Error_Com` API Examples:

**Complete API Reference**: See [`score/mw/com/impl/com_error.h`](../impl/com_error.h) for all available ComErrc error codes.

### Example 1: Using `ComErrc` for InstanceIdentifier Creation Error Handling

`ComErrc` values are used to form the Error part of a `score::Result`.
For example, a function that creates an InstanceIdentifier could have this form:

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

An actual in production example of InstanceIdentifier::Create looks as follows:

**Scenario**: Creating InstanceIdentifier from serialized JSON with proper error handling

**Location**: `score/mw/com/impl/instance_identifier.cpp`

```cpp
#include "score/mw/com/com_error_domain.h"

score::Result<InstanceIdentifier> InstanceIdentifier::Create(std::string_view serialized_format) noexcept
{
    // Check if static configuration has been initialized before use
    if (configuration_ == nullptr)
    {
        ::score::mw::log::LogFatal("lola") << "InstanceIdentifier configuration pointer hasn't been set. Exiting";
        // Return ComErrc::kInvalidConfiguration for missing configuration setup
        return MakeUnexpected(ComErrc::kInvalidConfiguration);
    }

    // Parse the serialized JSON string into a JSON object
    json::JsonParser json_parser{};
    auto json_result = json_parser.FromBuffer({serialized_format.data(), serialized_format.size()});
    if (!json_result.has_value())
    {
        ::score::mw::log::LogFatal("lola") << "InstanceIdentifier serialized string is invalid. Exiting";
        // Return ComErrc::kInvalidInstanceIdentifierString for JSON parsing failure
        return MakeUnexpected(ComErrc::kInvalidInstanceIdentifierString);
    }

    // Extract JSON object and construct InstanceIdentifier
    const auto& json_object = std::move(json_result).value().As<json::Object>().value().get();
    std::string serialized_string{serialized_format.data(), serialized_format.size()};
    InstanceIdentifier instance_identifier{json_object, std::move(serialized_string)};
    return instance_identifier;
}
```

**Key Points**:
- `ComErrc::kInvalidConfiguration`: Configuration pointer is null
- `ComErrc::kInvalidInstanceIdentifierString`: JSON parsing failure
- `MakeUnexpected(ComErrc)`: Converts ComErrc to `score::Result<T>` error state
- `noexcept`: Only standardized error codes, no exceptions thrown

---

### Example 2: Using `ComErrorDomain.MessageFor()` for Error Messages

``` cpp
auto error_message = ComErrorDomain.MessageFor(static_cast<score::result::ErrorCode>(score::mw::com::ComErrc::kServiceNotAvailable));
// error_message will contain: "Service is not available."
```
**Key Points**:
- `MessageFor()` converts error codes to human-readable messages
- Error codes must be cast to `score::result::ErrorCode` type
- Each error code has a predefined, standardized message
- Useful for logging and debugging

---
