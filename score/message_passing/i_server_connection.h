#ifndef SCORE_LIB_MESSAGE_PASSING_I_SERVER_CONNECTION_H
#define SCORE_LIB_MESSAGE_PASSING_I_SERVER_CONNECTION_H

#include "score/message_passing/server_types.h"

#include "score/os/errno.h"

#include <score/expected.hpp>
#include <score/span.hpp>

namespace score
{
namespace message_passing
{

/// \brief Interface of a Message Passing Server connection
/// \details The interface is providing the server side of asynchronous client-server IPC communication.
class IServerConnection
{
  public:
    virtual const ClientIdentity& GetClientIdentity() const noexcept = 0;
    virtual UserData& GetUserData() noexcept = 0;

    virtual score::cpp::expected_blank<score::os::Error> Reply(score::cpp::span<const std::uint8_t> message) noexcept = 0;
    virtual score::cpp::expected_blank<score::os::Error> Notify(score::cpp::span<const std::uint8_t> message) noexcept = 0;
    virtual void RequestDisconnect() noexcept = 0;

  protected:
    ~IServerConnection() noexcept = default;
};

}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_I_SERVER_CONNECTION_H
