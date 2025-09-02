#include "score/message_passing/qnx_dispatch/qnx_dispatch_server_factory.h"

#include "score/message_passing/qnx_dispatch/qnx_dispatch_engine.h"
#include "score/message_passing/qnx_dispatch/qnx_dispatch_server.h"

namespace score
{
namespace message_passing
{

QnxDispatchServerFactory::QnxDispatchServerFactory(score::cpp::pmr::memory_resource* const resource) noexcept
    : QnxDispatchServerFactory{score::cpp::pmr::make_shared<QnxDispatchEngine>(resource, resource)}
{
}

QnxDispatchServerFactory::QnxDispatchServerFactory(const std::shared_ptr<QnxDispatchEngine> engine) noexcept
    : engine_{engine}
{
}

QnxDispatchServerFactory::~QnxDispatchServerFactory() noexcept {}

score::cpp::pmr::unique_ptr<IServer> QnxDispatchServerFactory::Create(const ServiceProtocolConfig& protocol_config,
                                                               const ServerConfig& server_config) noexcept
{
    return score::cpp::pmr::make_unique<detail::QnxDispatchServer>(
        engine_->GetMemoryResource(), engine_, protocol_config, server_config);
}

}  // namespace message_passing
}  // namespace score
