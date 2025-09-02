#ifndef SCORE_LIB_MESSAGE_PASSING_CLIENT_SERVER_COMMUNICATION_H
#define SCORE_LIB_MESSAGE_PASSING_CLIENT_SERVER_COMMUNICATION_H

#include <cstdint>

namespace score
{
namespace message_passing
{
namespace detail
{

enum class ClientToServer : std::uint8_t
{
    SEND,
    REQUEST
};

enum class ServerToClient : std::uint8_t
{
    REPLY,
    NOTIFY
};

}  // namespace detail
}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_CLIENT_SERVER_COMMUNICATION_H
