#include "score/message_passing/qnx_dispatch/qnx_dispatch_client_factory.h"

#include "score/message_passing/client_connection.h"
#include "score/message_passing/qnx_dispatch/qnx_dispatch_engine.h"

namespace score
{
namespace message_passing
{

QnxDispatchClientFactory::QnxDispatchClientFactory(score::cpp::pmr::memory_resource* const resource) noexcept
    : QnxDispatchClientFactory{score::cpp::pmr::make_shared<QnxDispatchEngine>(resource, resource)}
{
}

QnxDispatchClientFactory::QnxDispatchClientFactory(const std::shared_ptr<QnxDispatchEngine> engine) noexcept
    : engine_{engine}
{
}

QnxDispatchClientFactory::~QnxDispatchClientFactory() noexcept {}

score::cpp::pmr::unique_ptr<IClientConnection> QnxDispatchClientFactory::Create(const ServiceProtocolConfig& protocol_config,
                                                                         const ClientConfig& client_config) noexcept
{
    return score::cpp::pmr::make_unique<detail::ClientConnection>(
        engine_->GetMemoryResource(), engine_, protocol_config, client_config);
}

}  // namespace message_passing
}  // namespace score
