#include "score/message_passing/qnx_dispatch/qnx_dispatch_server.h"

#include "score/message_passing/client_server_communication.h"
#include "score/message_passing/qnx_dispatch/qnx_dispatch_engine.h"

#include <score/utility.hpp>

namespace score
{
namespace message_passing
{
namespace detail
{

QnxDispatchServer::ServerConnection::ServerConnection(ClientIdentity client_identity,
                                                      const QnxDispatchServer& server) noexcept
    : client_identity_{client_identity},
      reply_message_{server.engine_->GetMemoryResource()},
      notify_storage_{server.server_config_.max_queued_notifies, server.engine_->GetMemoryResource()}
{
    reply_message_.message.reserve(server.max_reply_size_);
    reply_message_.code = score::cpp::to_underlying(ServerToClient::REPLY);

    for (auto& notify_message : notify_storage_)
    {
        notify_message.message.reserve(server.max_notify_size_);
        notify_message.code = score::cpp::to_underlying(ServerToClient::NOTIFY);
    }
    notify_pool_.assign(notify_storage_.begin(), notify_storage_.end());
}

void QnxDispatchServer::ServerConnection::ApproveConnection(UserData&& data,
                                                            score::cpp::pmr::unique_ptr<ServerConnection>&& self) noexcept
{
    user_data_ = std::move(data);
    self_ = std::move(self);
}

const ClientIdentity& QnxDispatchServer::ServerConnection::GetClientIdentity() const noexcept
{
    return client_identity_;
}

UserData& QnxDispatchServer::ServerConnection::GetUserData() noexcept
{
    return *user_data_;
}

score::cpp::expected_blank<score::os::Error> QnxDispatchServer::ServerConnection::Reply(
    score::cpp::span<const std::uint8_t> message) noexcept
{
    // attr member is inherited from iofunc_ocb_t base class
    auto& server = *static_cast<QnxDispatchServer*>(attr);

    if (message.size() > server.max_reply_size_)
    {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOMEM));
    }

    reply_message_.message.assign(message.begin(), message.end());

    send_queue_.push_back(reply_message_);
    auto& os_resources = server.engine_->GetOsResources();
    os_resources.iofunc->iofunc_notify_trigger(notify_, send_queue_.size(), IOFUNC_NOTIFY_INPUT);
    return {};
}

score::cpp::expected_blank<score::os::Error> QnxDispatchServer::ServerConnection::Notify(
    score::cpp::span<const std::uint8_t> message) noexcept
{
    auto& server = *static_cast<QnxDispatchServer*>(attr);

    if (message.size() > server.max_notify_size_)
    {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOMEM));
    }

    if (notify_pool_.empty())
    {
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(ENOMEM));
    }

    auto& notify_message = notify_pool_.front();
    notify_pool_.pop_front();
    notify_message.message.assign(message.begin(), message.end());
    send_queue_.push_back(notify_message);

    auto& os_resources = server.engine_->GetOsResources();
    os_resources.iofunc->iofunc_notify_trigger(notify_, send_queue_.size(), IOFUNC_NOTIFY_INPUT);
    return {};
}

void QnxDispatchServer::ServerConnection::RequestDisconnect() noexcept
{
    // TODO: implement as ionotify for zero-read
}

bool QnxDispatchServer::ServerConnection::ProcessInput(const std::uint8_t code,
                                                       const score::cpp::span<const std::uint8_t> message) noexcept
{
    auto& server = *static_cast<QnxDispatchServer*>(attr);
    auto& user_data = *user_data_;
    using HandlerPointerT = score::cpp::pmr::unique_ptr<IConnectionHandler>;
    switch (code)
    {
        case score::cpp::to_underlying(ClientToServer::REQUEST):
            if (score::cpp::holds_alternative<HandlerPointerT>(user_data))
            {
                score::cpp::get<HandlerPointerT>(user_data)->OnMessageSentWithReply(*this, message);
            }
            else
            {
                server.sent_with_reply_callback_(*this, message);
            }
            break;
        case score::cpp::to_underlying(ClientToServer::SEND):
            if (score::cpp::holds_alternative<HandlerPointerT>(user_data))
            {
                score::cpp::get<HandlerPointerT>(user_data)->OnMessageSent(*this, message);
            }
            else
            {
                server.sent_callback_(*this, message);
            }
            break;

        default:
            // unrecognised message; drop connection
            return false;
    }
    return true;
}

bool QnxDispatchServer::ServerConnection::HasSomethingToRead() noexcept
{
    return !send_queue_.empty();
}

std::int32_t QnxDispatchServer::ServerConnection::ProcessReadRequest(resmgr_context_t* const ctp) noexcept
{
    if (send_queue_.empty())
    {
        return EAGAIN;
    }

    auto& send_message = send_queue_.front();
    constexpr auto kVectorCount = 2UL;
    std::array<iov_t, kVectorCount> io;
    io[0].iov_base = &send_message.code;
    io[0].iov_len = sizeof(send_message.code);
    io[1].iov_base = send_message.message.data();
    io[1].iov_len = send_message.message.size();

    _IO_SET_READ_NBYTES(ctp, sizeof(send_message.code) + send_message.message.size());

    auto& server = *static_cast<QnxDispatchServer*>(attr);
    auto& os_resources = server.engine_->GetOsResources();

    // TODO: drop on error?
    // could also be dispatch->resmgr_msgreplyv(), if we decide to introduce that
    score::cpp::ignore = os_resources.channel->MsgReplyv(ctp->rcvid, ctp->status, io.data(), 2);
    send_queue_.pop_front();
    if (send_message.code == score::cpp::to_underlying(ServerToClient::NOTIFY))
    {
        // if a NOTIFY message instance, return it to notify pool
        notify_pool_.push_front(send_message);
    }
    return _RESMGR_NOREPLY;
}

void QnxDispatchServer::ServerConnection::ProcessDisconnect() noexcept
{
    self_.reset();
}

QnxDispatchServer::ServerConnection::~ServerConnection() noexcept
{
    auto& server = *static_cast<QnxDispatchServer*>(attr);
    if (user_data_.has_value())
    {
        auto& user_data = *user_data_;
        using HandlerPointerT = score::cpp::pmr::unique_ptr<IConnectionHandler>;
        if (score::cpp::holds_alternative<HandlerPointerT>(user_data))
        {
            score::cpp::get<HandlerPointerT>(user_data)->OnDisconnect(*this);
        }
        else
        {
            server.disconnect_callback_(*this);
        }
    }
    // TODO: if reusing connection instance, don't forget to restore notify_pool_
    send_queue_.clear();
    notify_pool_.clear();
}

QnxDispatchServer::QnxDispatchServer(std::shared_ptr<QnxDispatchEngine> engine,
                                     const ServiceProtocolConfig& protocol_config,
                                     const IServerFactory::ServerConfig& server_config) noexcept
    : ResourceManagerServer{std::move(engine)},
      identifier_{protocol_config.identifier.data(), protocol_config.identifier.size(), engine_->GetMemoryResource()},
      max_request_size_{protocol_config.max_send_size},
      max_reply_size_{protocol_config.max_reply_size},
      max_notify_size_{protocol_config.max_notify_size},
      server_config_{server_config}
{
}

QnxDispatchServer::~QnxDispatchServer() noexcept
{
    StopListening();
}

score::cpp::expected_blank<score::os::Error> QnxDispatchServer::StartListening(ConnectCallback connect_callback,
                                                                      DisconnectCallback disconnect_callback,
                                                                      MessageCallback sent_callback,
                                                                      MessageCallback sent_with_reply_callback) noexcept
{
    connect_callback_ = std::move(connect_callback);
    disconnect_callback_ = std::move(disconnect_callback);
    sent_callback_ = std::move(sent_callback);
    sent_with_reply_callback_ = std::move(sent_with_reply_callback);

    const QnxResourcePath path{identifier_};

    // ResourceManagerServer::
    return Start(path);
}

void QnxDispatchServer::StopListening() noexcept
{
    // ResourceManagerServer::
    Stop();
}

std::int32_t QnxDispatchServer::ProcessConnect(resmgr_context_t* const ctp, io_open_t* const msg) noexcept
{
    auto& os_resources = engine_->GetOsResources();
    auto& channel = os_resources.channel;
    auto& iofunc = os_resources.iofunc;

    // the attr locks are currently not needed, but we should not forget about them in multithreaded implementation
    iofunc->iofunc_attr_lock(&attr);
    score::cpp::expected_blank<std::int32_t> status = iofunc->iofunc_open(ctp, msg, &attr, 0, 0);
    if (!status.has_value())
    {
        iofunc->iofunc_attr_unlock(&attr);
        return status.error();
    }

    _client_info cinfo{};
    const auto result = channel->ConnectClientInfo(ctp->info.scoid, &cinfo, 0);
    if (!result.has_value())
    {
        return EINVAL;
    }
    ClientIdentity identity{ctp->info.pid, cinfo.cred.euid, cinfo.cred.egid};

    // here, we need to provide server directly as an argument
    auto connection = score::cpp::pmr::make_unique<ServerConnection>(engine_->GetMemoryResource(), identity, *this);

    auto data_expected = connect_callback_(*connection);
    if (!data_expected.has_value())
    {
        return EACCES;
    }

    status = iofunc->iofunc_ocb_attach(ctp, msg, connection.get(), &attr, 0);
    if (!status.has_value())
    {
        iofunc->iofunc_attr_unlock(&attr);
        return status.error();
    }
    // from now on, the connection can extract server from its iofunc_ocb_t base class

    connection->ApproveConnection(std::move(data_expected.value()), std::move(connection));

    iofunc->iofunc_attr_unlock(&attr);
    return EOK;
}

}  // namespace detail
}  // namespace message_passing
}  // namespace score
