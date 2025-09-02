#include "score/message_passing/qnx_dispatch/qnx_dispatch_engine.h"

#include "score/message_passing/qnx_dispatch/qnx_resource_path.h"

namespace score
{
namespace message_passing
{

namespace
{

constexpr std::int32_t kTimerPulseCode = _PULSE_CODE_MINAVAIL;
constexpr std::int32_t kEventPulseCode = _PULSE_CODE_MINAVAIL + 1;

}  // namespace

QnxDispatchEngine::QnxDispatchEngine(score::cpp::pmr::memory_resource* memory_resource) noexcept
    : memory_resource_{memory_resource},
      os_resources_{GetDefaultOsResources(memory_resource)},
      quit_flag_{false},
      posix_receive_buffer_{memory_resource}
{
    const auto dpp_expected = os_resources_.dispatch->dispatch_create_channel(-1, 0);
    if (!dpp_expected.has_value())
    {
        fprintf(stderr, "Unable to allocate dispatch handle.\n");
        return;  // TODO: ...
    }
    dispatch_pointer_ = dpp_expected.value();

    if (!os_resources_.dispatch->pulse_attach(dispatch_pointer_, 0, kTimerPulseCode, &TimerPulseCallback, this)
             .has_value())
    {
        fprintf(stderr, "Failed to attach code %d.\n", kTimerPulseCode);
    }
    if (!os_resources_.dispatch->pulse_attach(dispatch_pointer_, 0, kEventPulseCode, &EventPulseCallback, this)
             .has_value())
    {
        fprintf(stderr, "Failed to attach code %d.\n", kEventPulseCode);
    }

    /* resmgr_attach */

    const auto coid_expected = os_resources_.dispatch->message_connect(dispatch_pointer_, MSG_FLAG_SIDE_CHANNEL);
    side_channel_coid_ = coid_expected.value();

    struct sigevent event;
    event.sigev_notify = SIGEV_PULSE;
    event.sigev_coid = side_channel_coid_;
    event.sigev_priority = SIGEV_PULSE_PRIO_INHERIT;
    event.sigev_code = kTimerPulseCode;
    event.sigev_value.sival_int = 0;
    const auto timer_id_expected = os_resources_.timer->TimerCreate(CLOCK_MONOTONIC, &event);
    timer_id_ = timer_id_expected.value();

    // it's actually the default settings for resmgr buffers; we are just making them explicit here
    // Ourselves, we are not limited by these values, as we use resmgr_msgget() and we don't use ctp.iov
    resmgr_attr_t resmgr_attr_;
    resmgr_attr_.nparts_max = 1U;
    resmgr_attr_.msg_max_size = 2088U;
    score::cpp::ignore = os_resources_.dispatch->resmgr_attach(
        dispatch_pointer_, &resmgr_attr_, nullptr, _FTYPE_ANY, 0, nullptr, nullptr, nullptr);

    const auto ctp_expected = os_resources_.dispatch->dispatch_context_alloc(dispatch_pointer_);
    if (!ctp_expected.has_value())
    {
        fprintf(stderr, "Unable to allocate context pointer.\n");
        return;  // TODO: ...
    }
    context_pointer_ = ctp_expected.value();

    SetupResourceManagerCallbacks();

    std::lock_guard acquire{thread_mutex_};  // postpone thread start till we assign thread_
    thread_ = std::thread([this]() noexcept {
        {
            std::lock_guard release{thread_mutex_};
        }
        RunOnThread();
    });
}

int QnxDispatchEngine::EndpointFdSelectCallback(select_context_t* /*ctp*/,
                                                int /*fd*/,
                                                unsigned /*flags*/,
                                                void* handle) noexcept
{
    PosixEndpointEntry& endpoint = *static_cast<PosixEndpointEntry*>(handle);
    endpoint.input();
    return 0;
}

int QnxDispatchEngine::TimerPulseCallback(message_context_t* /*ctp*/,
                                          int /*code*/,
                                          unsigned /*flags*/,
                                          void* handle) noexcept
{
    static_cast<QnxDispatchEngine*>(handle)->ProcessTimerQueue();
    return 0;
}

int QnxDispatchEngine::EventPulseCallback(message_context_t* ctp,
                                          int /*code*/,
                                          unsigned /*flags*/,
                                          void* handle) noexcept
{
    static_cast<QnxDispatchEngine*>(handle)->ProcessPulseEvent(
        static_cast<PulseEvent>(ctp->msg->pulse.value.sival_int));
    return 0;
}

QnxDispatchEngine::~QnxDispatchEngine() noexcept
{
    SendPulseEvent(PulseEvent::QUIT);
    thread_.join();
    score::cpp::ignore = os_resources_.timer->TimerDestroy(timer_id_);
    score::cpp::ignore = os_resources_.channel->ConnectDetach(side_channel_coid_);
    score::cpp::ignore = os_resources_.dispatch->pulse_detach(dispatch_pointer_, kEventPulseCode, 0);
    score::cpp::ignore = os_resources_.dispatch->pulse_detach(dispatch_pointer_, kTimerPulseCode, 0);
    score::cpp::ignore = os_resources_.dispatch->dispatch_destroy(dispatch_pointer_);
    os_resources_.dispatch->dispatch_context_free(context_pointer_);
}

score::cpp::expected<std::int32_t, score::os::Error> QnxDispatchEngine::TryOpenClientConnection(
    score::cpp::string_view identifier) noexcept
{
    const detail::QnxResourcePath path{identifier};
    return os_resources_.fcntl->open(path.c_str(), score::os::Fcntl::Open::kReadWrite);
}

void QnxDispatchEngine::CloseClientConnection(std::int32_t client_fd) noexcept
{
    os_resources_.unistd->close(client_fd);
}

void QnxDispatchEngine::RegisterPosixEndpoint(PosixEndpointEntry& endpoint) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(IsOnCallbackThread());

    if (posix_receive_buffer_.size() < endpoint.max_receive_size)
    {
        posix_receive_buffer_.resize(endpoint.max_receive_size);
    }

    os_resources_.dispatch->select_attach(dispatch_pointer_,
                                          nullptr,
                                          endpoint.fd,
                                          SELECT_FLAG_READ | SELECT_FLAG_REARM,
                                          &EndpointFdSelectCallback,
                                          &endpoint);
    posix_endpoint_list_.push_back(endpoint);
}

void QnxDispatchEngine::UnregisterPosixEndpoint(PosixEndpointEntry& endpoint) noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(IsOnCallbackThread());

    posix_endpoint_list_.erase(posix_endpoint_list_.iterator_to(endpoint));
    UnselectEndpoint(endpoint);
}

void QnxDispatchEngine::UnselectEndpoint(PosixEndpointEntry& endpoint) noexcept
{
    score::cpp::ignore = os_resources_.dispatch->select_detach(dispatch_pointer_, endpoint.fd);
    if (!endpoint.disconnect.empty())
    {
        endpoint.disconnect();
    }
}

void QnxDispatchEngine::EnqueueCommand(CommandQueueEntry& entry,
                                       const TimePoint until,
                                       CommandCallback callback,
                                       const void* const owner) noexcept
{
    timer_queue_.RegisterTimedEntry(entry, until, std::move(callback), owner);
    SendPulseEvent(PulseEvent::TIMER);
}

void QnxDispatchEngine::CleanUpOwner(const void* const owner) noexcept
{
    if (owner == nullptr)
    {
        return;
    }
    if (IsOnCallbackThread())
    {
        ProcessCleanup(owner);
    }
    else
    {
        std::unique_lock lock{thread_mutex_};  // non-allocating replacement for void promise
        bool done = false;
        detail::TimedCommandQueue::Entry cleanup_command;
        timer_queue_.RegisterImmediateEntry(
            cleanup_command,
            [this, owner, &done](auto) noexcept {
                ProcessCleanup(owner);
                {
                    std::lock_guard guard{thread_mutex_};
                    done = true;
                }
                thread_condition_.notify_all();
            },
            owner);
        SendPulseEvent(PulseEvent::TIMER);
        thread_condition_.wait(lock, [&done]() {
            return done;
        });
    }
}

score::cpp::expected_blank<score::os::Error> QnxDispatchEngine::SendProtocolMessage(
    const std::int32_t fd,
    std::uint8_t code,
    const score::cpp::span<const std::uint8_t> message) noexcept
{
    constexpr auto kVectorCount = 2UL;
    std::array<iov_t, kVectorCount> io;
    io[0].iov_base = &code;
    io[0].iov_len = sizeof(code);
    io[1].iov_base = const_cast<std::uint8_t*>(message.data());
    io[1].iov_len = static_cast<std::size_t>(message.size());

    auto result_expected = os_resources_.uio->writev(fd, io.data(), io.size());
    if (result_expected.has_value())
    {
        return {};
    }
    return score::cpp::make_unexpected(result_expected.error());
}

score::cpp::expected<score::cpp::span<const std::uint8_t>, score::os::Error> QnxDispatchEngine::ReceiveProtocolMessage(
    const std::int32_t fd,
    std::uint8_t& code) noexcept
{
    auto size_expected = os_resources_.unistd->read(fd, posix_receive_buffer_.data(), posix_receive_buffer_.size());
    if (!size_expected.has_value())
    {
        return score::cpp::make_unexpected(size_expected.error());
    }
    ssize_t size = size_expected.value();
    if (size < 1)
    {
        // other side disconnected
        return score::cpp::make_unexpected(score::os::Error::createFromErrno(EPIPE));
    }
    code = posix_receive_buffer_[0];
    return score::cpp::span<const std::uint8_t>{posix_receive_buffer_.data() + 1, static_cast<std::size_t>(size - 1)};
}

void QnxDispatchEngine::SendPulseEvent(const PulseEvent pulse_event) noexcept
{
    score::cpp::ignore = os_resources_.channel->MsgSendPulse(
        side_channel_coid_, -1, kEventPulseCode, static_cast<std::int32_t>(pulse_event));
}

void QnxDispatchEngine::ProcessPulseEvent(const PulseEvent pulse_event) noexcept
{
    if (pulse_event == PulseEvent::TIMER)
    {
        ProcessTimerQueue();
    }
    else
    {
        quit_flag_ = true;
    }
}

void QnxDispatchEngine::ProcessCleanup(const void* const owner) noexcept
{
    posix_endpoint_list_.remove_and_dispose_if(
        [owner](auto& endpoint) noexcept {
            return endpoint.owner == owner;
        },
        [this](auto* endpoint) noexcept {
            UnselectEndpoint(*endpoint);
        });

    timer_queue_.CleanUpOwner(owner);
}

void QnxDispatchEngine::RunOnThread() noexcept
{
    while (!quit_flag_)
    {
        if (os_resources_.dispatch->dispatch_block(context_pointer_))
        {
            score::cpp::ignore = os_resources_.dispatch->dispatch_handler(context_pointer_);
        }
    }
}

void QnxDispatchEngine::ProcessTimerQueue() noexcept
{
    const auto now = Clock::now();
    const auto then = timer_queue_.ProcessQueue(now);
    if (then == TimePoint{})
    {
        // no future event to wait for (yet); no need to re-arm timer
        return;
    }
    const auto distance = std::chrono::duration_cast<std::chrono::nanoseconds>(then - Clock::now()).count() + 1;
    struct _itimer itimer;
    itimer.nsec = distance;
    itimer.interval_nsec = 0;
    score::cpp::ignore = os_resources_.timer->TimerSettime(timer_id_, 0, &itimer, NULL);
}

void QnxDispatchEngine::SetupResourceManagerCallbacks() noexcept
{
    // pre-configure resmgr callback data
    os_resources_.iofunc->iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &connect_funcs_, _RESMGR_IO_NFUNCS, &io_funcs_);
    connect_funcs_.open = &io_open;
    io_funcs_.notify = &io_notify;
    io_funcs_.write = &io_write;
    io_funcs_.read = &io_read;
    io_funcs_.close_ocb = &io_close_ocb;
}

score::cpp::expected_blank<score::os::Error> QnxDispatchEngine::StartServer(ResourceManagerServer& server,
                                                                   const QnxResourcePath& path) noexcept
{
    // QNX defect PR# 2561573: resmgr_attach/message_attach calls are not thread-safe for the same dispatch_pointer
    std::lock_guard guard{attach_mutex_};

    const auto id_expected = os_resources_.dispatch->resmgr_attach(dispatch_pointer_,
                                                                   nullptr,
                                                                   path.c_str(),
                                                                   _FTYPE_ANY,
                                                                   static_cast<std::uint32_t>(_RESMGR_FLAG_SELF),
                                                                   &connect_funcs_,
                                                                   &io_funcs_,
                                                                   &server);
    if (!id_expected.has_value())
    {
        return score::cpp::make_unexpected(id_expected.error());
    }
    server.resmgr_id_ = id_expected.value();

    // pre-configure resmgr access rights data; attr member is from the extended_dev_attr_t base class
    constexpr mode_t attrMode{S_IFNAM | 0666};
    os_resources_.iofunc->iofunc_attr_init(&server.attr, attrMode, nullptr, nullptr);

    return {};
}

void QnxDispatchEngine::StopServer(ResourceManagerServer& server) noexcept
{
    if (server.resmgr_id_ != -1)
    {
        // QNX defect PR# 2561573: resmgr_attach/message_attach calls are not thread-safe for the same dispatch_pointer
        std::lock_guard guard{attach_mutex_};
        const auto detach_expected = os_resources_.dispatch->resmgr_detach(
            dispatch_pointer_, server.resmgr_id_, static_cast<std::uint32_t>(_RESMGR_DETACH_CLOSE));
        server.resmgr_id_ = -1;
    }
}

std::int32_t QnxDispatchEngine::io_open(resmgr_context_t* const ctp,
                                        io_open_t* const msg,
                                        RESMGR_HANDLE_T* const handle,
                                        void* const /*extra*/) noexcept
{
    ResourceManagerServer& server = ResmgrHandleToServer(handle);

    return server.ProcessConnect(ctp, msg);
}

std::int32_t QnxDispatchEngine::io_write(resmgr_context_t* const ctp,
                                         io_write_t* const msg,
                                         RESMGR_OCB_T* const ocb) noexcept
{
    QnxDispatchEngine& self = *OcbToServer(ocb).engine_;
    auto& iofunc = self.GetOsResources().iofunc;
    auto& dispatch = self.GetOsResources().dispatch;

    // check if the write operation is allowed
    auto result = iofunc->iofunc_write_verify(ctp, msg, ocb, nullptr);
    if (!result.has_value())
    {
        return result.error();
    }

    // check if we are requested to do just a plain write
    if ((msg->i.xtype & _IO_XTYPE_MASK) != _IO_XTYPE_NONE)
    {
        return ENOSYS;
    }

    // get the number of bytes we were asked to write, check that there are enough bytes in the message
    const std::size_t nbytes = _IO_WRITE_GET_NBYTES(msg);  // LCOV_EXCL_BR_LINE library macro with benign conditional
    const std::size_t nbytes_max = static_cast<std::size_t>(ctp->info.srcmsglen) - ctp->offset - sizeof(io_write_t);
    if (nbytes > nbytes_max)
    {
        return EBADMSG;
    }

    // take from engine and control nbytes
    std::array<std::uint8_t, 2048> buffer;

    auto msgget_expected = dispatch->resmgr_msgget(ctp, buffer.data(), nbytes, sizeof(msg->i));
    if (!msgget_expected.has_value())
    {
        return msgget_expected.error().GetOsDependentErrorCode();
    }

    const std::uint8_t code = buffer[0];
    const score::cpp::span<const std::uint8_t> message = {buffer.data() + 1,
                                                   static_cast<score::cpp::span<std::uint8_t>::size_type>(nbytes - 1)};

    // TODO: close connection on false
    score::cpp::ignore = OcbToConnection(ocb).ProcessInput(code, message);

    _IO_SET_WRITE_NBYTES(ctp, static_cast<std::int64_t>(nbytes));
    return EOK;
}

std::int32_t QnxDispatchEngine::io_read(resmgr_context_t* const ctp,
                                        io_read_t* const msg,
                                        RESMGR_OCB_T* const ocb) noexcept
{
    QnxDispatchEngine& self = *OcbToServer(ocb).engine_;
    auto& iofunc = self.GetOsResources().iofunc;

    auto result = iofunc->iofunc_read_verify(ctp, msg, ocb, nullptr);
    if (!result.has_value())
    {
        return result.error();
    }

    if ((msg->i.xtype & _IO_XTYPE_MASK) != _IO_XTYPE_NONE)
    {
        return ENOSYS;
    }

    size_t nbytes = _IO_READ_GET_NBYTES(msg);
    if (nbytes == 0)
    {
        _IO_SET_READ_NBYTES(ctp, 0);
        return _RESMGR_NPARTS(0);
    }

    ResourceManagerConnection& connection = OcbToConnection(ocb);
    return connection.ProcessReadRequest(ctp);
}

std::int32_t QnxDispatchEngine::io_notify(resmgr_context_t* const ctp,
                                          io_notify_t* const msg,
                                          RESMGR_OCB_T* const ocb) noexcept
{
    QnxDispatchEngine& self = *OcbToServer(ocb).engine_;
    auto& iofunc = self.GetOsResources().iofunc;

    ResourceManagerConnection& connection = OcbToConnection(ocb);

    // 'trig' will tell iofunc_notify() which conditions are currently satisfied.
    std::int32_t trig;

    trig = _NOTIFY_COND_OUTPUT; /* clients can always give us data */
    if (connection.HasSomethingToRead())
    {
        trig = _NOTIFY_COND_INPUT; /* we have some data available */
    }
    return iofunc->iofunc_notify(ctp, msg, connection.notify_, trig, NULL, NULL);
}

std::int32_t QnxDispatchEngine::io_close_ocb(resmgr_context_t* const ctp,
                                             void* const /*reserved*/,
                                             RESMGR_OCB_T* const ocb) noexcept
{
    QnxDispatchEngine& self = *OcbToServer(ocb).engine_;
    auto& iofunc = self.GetOsResources().iofunc;

    ResourceManagerConnection& connection = OcbToConnection(ocb);

    iofunc->iofunc_notify_trigger_strict(ctp, connection.notify_, INT_MAX, IOFUNC_NOTIFY_INPUT);
    iofunc->iofunc_notify_trigger_strict(ctp, connection.notify_, INT_MAX, IOFUNC_NOTIFY_OUTPUT);
    iofunc->iofunc_notify_trigger_strict(ctp, connection.notify_, INT_MAX, IOFUNC_NOTIFY_OBAND);

    iofunc->iofunc_notify_remove(ctp, connection.notify_);

    // the attr locks are currently not needed, but we should not forget about them in multithreaded implementation
    iofunc->iofunc_attr_lock(&ocb->attr->attr);
    score::cpp::ignore = iofunc->iofunc_ocb_detach(ctp, ocb);
    iofunc->iofunc_attr_unlock(&ocb->attr->attr);

    connection.ProcessDisconnect();
    return EOK;
}

}  // namespace message_passing
}  // namespace score
