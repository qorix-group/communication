#ifndef SCORE_LIB_MESSAGE_PASSING_SERVER_TYPES_H
#define SCORE_LIB_MESSAGE_PASSING_SERVER_TYPES_H

#include "score/message_passing/i_connection_handler.h"

#include <score/callback.hpp>
#include <score/memory.hpp>
#include <score/variant.hpp>

namespace score
{
namespace message_passing
{

using UserData = score::cpp::variant<void*, std::uintptr_t, score::cpp::pmr::unique_ptr<IConnectionHandler>>;

using ConnectCallback =
    score::cpp::callback<score::cpp::expected<UserData, score::os::Error>(IServerConnection& connection) /* noexcept */>;

using DisconnectCallback = score::cpp::callback<void(IServerConnection& connection) /* noexcept */>;

using MessageCallback =
    score::cpp::callback<score::cpp::expected_blank<score::os::Error>(IServerConnection& connection,
                                                      score::cpp::span<const std::uint8_t> message) /* noexcept */>;

struct ClientIdentity
{
    pid_t pid;
    uid_t uid;
    gid_t gid;
};

}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_SERVER_TYPES_H
