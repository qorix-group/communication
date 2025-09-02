#ifndef SCORE_LIB_MESSAGE_PASSING_SERVICE_PROTOCOL_CONFIG_H
#define SCORE_LIB_MESSAGE_PASSING_SERVICE_PROTOCOL_CONFIG_H

#include <score/string_view.hpp>

#include <cstdint>
#include <memory>

namespace score
{
namespace message_passing
{

/// \brief The common part of the library configuration of a server and a client
struct ServiceProtocolConfig
{
    score::cpp::string_view identifier;    ///< The server name in the service namespace
    std::uint32_t max_send_size;    ///< Maximum size in bytes for the message from client to server
    std::uint32_t max_reply_size;   ///< Maximum size in bytes for the reply from server to client
    std::uint32_t max_notify_size;  ///< Maximum size in bytes for the notification message from server to client
};

}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_SERVICE_PROTOCOL_CONFIG_H
