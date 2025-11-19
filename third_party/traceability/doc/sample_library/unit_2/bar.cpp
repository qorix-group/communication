#include "third_party/traceability/doc/sample_library/unit_2/bar.h"

#include <cassert>

namespace
{
std::uint8_t kExpectedNumber = 42u;
}

namespace unit_2
{

Bar::Bar(std::unique_ptr<unit_1::Foo> foo) : foo_{std::move(foo)} {}

bool Bar::AssertNumber() const
{
    assert(foo_ != nullptr);
    return foo_->GetNumber() == kExpectedNumber;
}
}  // namespace unit_2
