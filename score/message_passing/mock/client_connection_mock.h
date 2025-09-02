#ifndef SCORE_LIB_MESSAGE_PASSING_MOCK_CLIENT_CONNECTION_MOCK_H
#define SCORE_LIB_MESSAGE_PASSING_MOCK_CLIENT_CONNECTION_MOCK_H

#include "score/message_passing/i_client_connection.h"

#include "gmock/gmock.h"

namespace score
{
namespace message_passing
{

class ClientConnectionMock final : public IClientConnection
{
  public:
    MOCK_METHOD(score::cpp::expected_blank<score::os::Error>, Send, (score::cpp::span<const std::uint8_t>), (noexcept, override));
    MOCK_METHOD((score::cpp::expected<score::cpp::span<std::uint8_t>, score::os::Error>),
                SendWaitReply,
                (score::cpp::span<const std::uint8_t>, score::cpp::span<std::uint8_t>),
                (noexcept, override));
    MOCK_METHOD(score::cpp::expected_blank<score::os::Error>,
                SendWithCallback,
                (score::cpp::span<const std::uint8_t>, ReplyCallback),
                (noexcept, override));
    MOCK_METHOD(State, GetState, (), (const, noexcept, override));
    MOCK_METHOD(StopReason, GetStopReason, (), (const, noexcept, override));
    MOCK_METHOD(void, Start, (StateCallback, NotifyCallback), (noexcept, override));
    MOCK_METHOD(void, Stop, (), (noexcept, override));
    MOCK_METHOD(void, Restart, (), (noexcept, override));

    // mockable virtual destructor
    MOCK_METHOD(void, Destruct, (), (noexcept));
    ~ClientConnectionMock() override
    {
        Destruct();
    }
};

}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_MOCK_CLIENT_CONNECTION_MOCK_H
