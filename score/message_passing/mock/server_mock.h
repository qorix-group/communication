#ifndef SCORE_MW_COM_MESSAGE_PASSING_NEW_SERVER_MOCK_H
#define SCORE_MW_COM_MESSAGE_PASSING_NEW_SERVER_MOCK_H

#include "score/message_passing/i_server.h"

#include "gmock/gmock.h"

namespace score
{
namespace message_passing
{

class ServerMock final : public IServer
{
  public:
    MOCK_METHOD(score::cpp::expected_blank<score::os::Error>,
                StartListening,
                (ConnectCallback, DisconnectCallback, MessageCallback, MessageCallback),
                (noexcept, override));
    MOCK_METHOD(void, StopListening, (), (noexcept, override));

    // mockable virtual destructor
    MOCK_METHOD(void, Destruct, (), (noexcept));
    ~ServerMock() override
    {
        Destruct();
    }
};

}  // namespace message_passing
}  // namespace score
#endif  // SCORE_MW_COM_MESSAGE_PASSING_NEW_SERVER_MOCK_H
