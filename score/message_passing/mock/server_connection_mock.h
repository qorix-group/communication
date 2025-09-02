#ifndef SCORE_LIB_MESSAGE_PASSING_MOCK_SERVER_CONNECTION_MOCK_H
#define SCORE_LIB_MESSAGE_PASSING_MOCK_SERVER_CONNECTION_MOCK_H

#include "score/message_passing/i_server_connection.h"

#include "gmock/gmock.h"

namespace score
{
namespace message_passing
{

class ServerConnectionMock final : public IServerConnection
{
  public:
    MOCK_METHOD(const ClientIdentity&, GetClientIdentity, (), (const, noexcept, override));
    MOCK_METHOD(UserData&, GetUserData, (), (noexcept, override));
    MOCK_METHOD(score::cpp::expected_blank<score::os::Error>, Reply, (score::cpp::span<const std::uint8_t>), (noexcept, override));
    MOCK_METHOD(score::cpp::expected_blank<score::os::Error>, Notify, (score::cpp::span<const std::uint8_t>), (noexcept, override));
    MOCK_METHOD(void, RequestDisconnect, (), (noexcept, override));
};

}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_MOCK_SERVER_CONNECTION_MOCK_H
