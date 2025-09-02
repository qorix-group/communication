#ifndef SCORE_LIB_MESSAGE_PASSING_QNX_DISPATCH_QNX_DISPATCH_CLIENT_FACTORY_H
#define SCORE_LIB_MESSAGE_PASSING_QNX_DISPATCH_QNX_DISPATCH_CLIENT_FACTORY_H

#include "score/message_passing/i_client_factory.h"

namespace score
{
namespace message_passing
{

class QnxDispatchEngine;

class QnxDispatchClientFactory final : IClientFactory
{
  public:
    explicit QnxDispatchClientFactory(
        score::cpp::pmr::memory_resource* const resource = score::cpp::pmr::get_default_resource()) noexcept;
    explicit QnxDispatchClientFactory(const std::shared_ptr<QnxDispatchEngine> engine) noexcept;
    ~QnxDispatchClientFactory() noexcept;

    score::cpp::pmr::unique_ptr<IClientConnection> Create(const ServiceProtocolConfig& protocol_config,
                                                   const ClientConfig& client_config) noexcept override;

    std::shared_ptr<QnxDispatchEngine> GetEngine() const noexcept
    {
        return engine_;
    }

  private:
    const std::shared_ptr<QnxDispatchEngine> engine_;
};

}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_QNX_DISPATCH_QNX_DISPATCH_CLIENT_FACTORY_H
