#include "third_party/traceability/doc/sample_library/unit_1/foo.h"

namespace unit_1
{

// trace: SampleLibrary.SampleRequirement

std::uint8_t Foo::GetNumber() const
{
    return 42u;
}
}  // namespace unit_1
