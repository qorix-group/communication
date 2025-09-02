#ifndef SCORE_LIB_MESSAGE_PASSING_I_CONNECTION_HANDLER_H
#define SCORE_LIB_MESSAGE_PASSING_I_CONNECTION_HANDLER_H

#include "score/os/errno.h"

#include <score/expected.hpp>
#include <score/span.hpp>

namespace score
{
namespace message_passing
{

// forward declaration
class IServerConnection;

/// \brief The user object to handle server connections
class IConnectionHandler
{
  public:
    virtual ~IConnectionHandler() = default;

    virtual score::cpp::expected_blank<score::os::Error> OnMessageSent(IServerConnection& connection,
                                                              score::cpp::span<const std::uint8_t> message) noexcept = 0;
    virtual score::cpp::expected_blank<score::os::Error> OnMessageSentWithReply(
        IServerConnection& connection,
        score::cpp::span<const std::uint8_t> message) noexcept = 0;
    virtual void OnDisconnect(IServerConnection& connection) noexcept = 0;
};

}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_I_CONNECTION_HANDLER_H
