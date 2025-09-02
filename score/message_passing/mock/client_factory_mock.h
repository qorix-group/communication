#ifndef SCORE_LIB_MESSAGE_PASSING_MOCK_CLIENT_FACTORY_MOCK_H
#define SCORE_LIB_MESSAGE_PASSING_MOCK_CLIENT_FACTORY_MOCK_H

#include "score/message_passing/i_client_factory.h"

#include "gmock/gmock.h"

namespace score
{
namespace message_passing
{

class ClientFactoryMock final : public IClientFactory
{
  public:
    MOCK_METHOD(score::cpp::pmr::unique_ptr<IClientConnection>,
                Create,
                (const ServiceProtocolConfig&, const ClientConfig&),
                (noexcept, override));
};

}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_MOCK_CLIENT_FACTORY_MOCK_H
