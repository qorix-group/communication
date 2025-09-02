#ifndef SCORE_LIB_MESSAGE_PASSING_UNIX_DOMAIN_UNIX_DOMAIN_SERVER_FACTORY_H
#define SCORE_LIB_MESSAGE_PASSING_UNIX_DOMAIN_UNIX_DOMAIN_SERVER_FACTORY_H

#include "score/message_passing/i_server_factory.h"

namespace score
{
namespace message_passing
{

class UnixDomainEngine;

class UnixDomainServerFactory final : IServerFactory
{
  public:
    explicit UnixDomainServerFactory(
        score::cpp::pmr::memory_resource* const resource = score::cpp::pmr::get_default_resource()) noexcept;
    explicit UnixDomainServerFactory(const std::shared_ptr<UnixDomainEngine> engine) noexcept;
    ~UnixDomainServerFactory() noexcept;

    score::cpp::pmr::unique_ptr<IServer> Create(const ServiceProtocolConfig& protocol_config,
                                         const ServerConfig& server_config) noexcept override;

    std::shared_ptr<UnixDomainEngine> GetEngine() const noexcept
    {
        return engine_;
    }

  private:
    const std::shared_ptr<UnixDomainEngine> engine_;
};

}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_UNIX_DOMAIN_UNIX_DOMAIN_SERVER_FACTORY_H
