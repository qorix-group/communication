#ifndef FOO_H
#define FOO_H

#include <cstdint>

namespace unit_1
{

class Foo final
{
  public:
    std::uint8_t GetNumber() const;
    void SetNumber(std::uint8_t value);
};

}  // namespace unit_1

#endif  // FOO_H
