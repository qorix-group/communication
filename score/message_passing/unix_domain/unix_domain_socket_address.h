#ifndef SCORE_LIB_MESSAGE_PASSING_UNIX_DOMAIN_UNIX_DOMAIN_SOCKET_ADDRESS_H
#define SCORE_LIB_MESSAGE_PASSING_UNIX_DOMAIN_UNIX_DOMAIN_SOCKET_ADDRESS_H

#include <score/string_view.hpp>

#include <sys/socket.h>
#include <sys/un.h>

namespace score
{
namespace message_passing
{
namespace detail
{

class UnixDomainSocketAddress
{
  public:
    UnixDomainSocketAddress(score::cpp::string_view path, bool isAbstract) noexcept;
    const char* GetAddressString() const noexcept
    {
        return &(addr_.sun_path[IsAbstract() ? 1 : 0]);
    }

    bool IsAbstract() const noexcept
    {
        return !addr_.sun_path[0];
    }

    const sockaddr* data() const noexcept
    {
        return reinterpret_cast<const sockaddr*>(&addr_);
    }
    static constexpr std::size_t size() noexcept
    {
        return sizeof(addr_);
    }

  private:
    struct sockaddr_un addr_;
};

inline UnixDomainSocketAddress::UnixDomainSocketAddress(const score::cpp::string_view path, const bool isAbstract) noexcept
{
    std::memset(static_cast<void*>(&addr_), 0, sizeof(addr_));
    addr_.sun_family = static_cast<sa_family_t>(AF_UNIX);
    const size_t len = std::min(sizeof(addr_.sun_path) - 1U - (isAbstract ? 1U : 0U), path.size());
    std::memcpy(addr_.sun_path + (isAbstract ? 1 : 0), path.data(), len);
}

}  // namespace detail
}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_UNIX_DOMAIN_UNIX_DOMAIN_SOCKET_ADDRESS_H
