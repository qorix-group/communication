#ifndef SCORE_LIB_MESSAGE_PASSING_MOCK_SERVER_FACTORY_MOCK_H
#define SCORE_LIB_MESSAGE_PASSING_MOCK_SERVER_FACTORY_MOCK_H

#include "score/message_passing/i_server_factory.h"

#include "gmock/gmock.h"

namespace score
{
namespace message_passing
{

class ServerFactoryMock final : public IServerFactory
{
  public:
    MOCK_METHOD(score::cpp::pmr::unique_ptr<IServer>,
                Create,
                (const ServiceProtocolConfig&, const ServerConfig&),
                (noexcept, override));
};

}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_MOCK_SERVER_FACTORY_MOCK_H
