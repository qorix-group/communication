#ifndef BAR_H
#define BAR_H

#include "third_party/traceability/doc/sample_library/unit_1/foo.h"

#include <cstdint>
#include <memory>

namespace unit_2
{

// trace: SampleLibrary.SampleRequirement
class Bar final
{
  public:
    explicit Bar(std::unique_ptr<unit_1::Foo>);
    bool AssertNumber() const;

  private:
    std::unique_ptr<unit_1::Foo> foo_;
};

}  // namespace unit_2

#endif  // BAR_H
