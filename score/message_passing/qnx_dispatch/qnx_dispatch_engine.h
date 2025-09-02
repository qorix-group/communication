#ifndef SCORE_LIB_MESSAGE_PASSING_QNX_DISPATCH_QNX_DISPATCH_ENGINE_H
#define SCORE_LIB_MESSAGE_PASSING_QNX_DISPATCH_QNX_DISPATCH_ENGINE_H

#include <score/callback.hpp>
#include <score/deque.hpp>
#include <score/memory.hpp>
#include <score/span.hpp>
#include <score/vector.hpp>

#include "score/message_passing/i_shared_resource_engine.h"
#include "score/message_passing/qnx_dispatch/qnx_resource_path.h"
#include "score/message_passing/timed_command_queue.h"
#include "score/os/fcntl.h"
#include "score/os/qnx/channel.h"
#include "score/os/qnx/dispatch.h"
#include "score/os/qnx/iofunc.h"
#include "score/os/qnx/timer_impl.h"
#include "score/os/sys_uio.h"
#include "score/os/unistd.h"

#include <condition_variable>
#include <mutex>
#include <thread>

namespace score
{
namespace message_passing
{

/// \brief Class encapsulating resources needed for QNX Dispatch Client/Server implementation
/// \details The class provides access to the OSAL resource objects, memory resource, background thread with poll loop,
///          and timer queue. It also provides an implementation of a simple message exchange transport protocol
///          over a connected socket.
///          The class is supposed to be shared via std::shared_ptr between its consumers (client and server factories,
///          server objects, client and server connections).
///          One or more instances of this class, with separate background threads and potentially separate memory
///          resources, can co-exist in the same process, if needed.
class QnxDispatchEngine final : public ISharedResourceEngine
{
  public:
    struct OsResources
    {
        score::cpp::pmr::unique_ptr<score::os::Channel> channel{};
        score::cpp::pmr::unique_ptr<score::os::Dispatch> dispatch{};
        score::cpp::pmr::unique_ptr<score::os::Fcntl> fcntl{};
        score::cpp::pmr::unique_ptr<score::os::IoFunc> iofunc{};
        score::cpp::pmr::unique_ptr<score::os::qnx::Timer> timer{};
        score::os::SysUio* uio{};  // TODO: implement uio OSAL "properly"?
        score::cpp::pmr::unique_ptr<score::os::Unistd> unistd{};
    };

    using QnxResourcePath = detail::QnxResourcePath;

    // we don't need "extended" attributes for us, but the OSAL is written in such a way...
    // TODO: remove "public"
    class ResourceManagerServer : public extended_dev_attr_t
    {
      public:
        ResourceManagerServer(std::shared_ptr<QnxDispatchEngine> engine) noexcept
            : engine_{std::move(engine)}, resmgr_id_{-1}
        {
        }

        score::cpp::expected_blank<score::os::Error> Start(const QnxResourcePath& path) noexcept
        {
            return engine_->StartServer(*this, path);
        }

        void Stop() noexcept
        {
            engine_->StopServer(*this);
        }

      private:
      public:  // TODO: remove
        friend QnxDispatchEngine;

        // TODO: isolate resmgr stuff
        virtual std::int32_t ProcessConnect(resmgr_context_t* const ctp, io_open_t* const msg) noexcept = 0;

        std::shared_ptr<QnxDispatchEngine> engine_;
        std::int32_t resmgr_id_;
    };

    // TODO: remove "public"
    class ResourceManagerConnection : public iofunc_ocb_t
    {
      public:
        ResourceManagerConnection() noexcept
        {
            IOFUNC_NOTIFY_INIT(notify_);
        }

      private:
        friend QnxDispatchEngine;

        virtual bool ProcessInput(const std::uint8_t code, const score::cpp::span<const std::uint8_t> message) noexcept = 0;
        virtual void ProcessDisconnect() noexcept = 0;
        virtual bool HasSomethingToRead() noexcept = 0;

        // TODO: isolate resmgr stuff
        virtual std::int32_t ProcessReadRequest(resmgr_context_t* const ctp) noexcept = 0;

        // TODO: private
      public:
        iofunc_notify_t notify_[3];
    };

    QnxDispatchEngine(score::cpp::pmr::memory_resource* memory_resource) noexcept;
    ~QnxDispatchEngine() noexcept;

    QnxDispatchEngine(const QnxDispatchEngine&) = delete;
    QnxDispatchEngine& operator=(const QnxDispatchEngine&) = delete;

    static OsResources GetDefaultOsResources(score::cpp::pmr::memory_resource* const memory_resource) noexcept
    {
        return {score::os::Channel::Default(memory_resource),
                score::os::Dispatch::Default(memory_resource),
                score::os::Fcntl::Default(memory_resource),
                score::os::IoFunc::Default(memory_resource),
                score::cpp::pmr::make_unique<score::os::qnx::TimerImpl>(memory_resource),
                &score::os::SysUio::instance(),
                score::os::Unistd::Default(memory_resource)};
    }

    score::cpp::pmr::memory_resource* GetMemoryResource() noexcept override
    {
        return memory_resource_;
    }
    OsResources& GetOsResources() noexcept
    {
        return os_resources_;
    }

    using FinalizeOwnerCallback = score::cpp::callback<void() /* noexcept */>;

    score::cpp::expected<std::int32_t, score::os::Error> TryOpenClientConnection(score::cpp::string_view identifier) noexcept override;

    void CloseClientConnection(std::int32_t client_fd) noexcept override;

    void RegisterPosixEndpoint(PosixEndpointEntry& endpoint) noexcept override;
    void UnregisterPosixEndpoint(PosixEndpointEntry& endpoint) noexcept override;

    void EnqueueCommand(CommandQueueEntry& entry,
                        const TimePoint until,
                        CommandCallback callback,
                        const void* const owner = nullptr) noexcept override;

    // this call is blocking
    void CleanUpOwner(const void* const owner) noexcept override;

    score::cpp::expected_blank<score::os::Error> SendProtocolMessage(
        const std::int32_t fd,
        std::uint8_t code,
        const score::cpp::span<const std::uint8_t> message) noexcept override;
    score::cpp::expected<score::cpp::span<const std::uint8_t>, score::os::Error> ReceiveProtocolMessage(
        const std::int32_t fd,
        std::uint8_t& code) noexcept override;

    bool IsOnCallbackThread() const noexcept override
    {
        return std::this_thread::get_id() == thread_.get_id();
    }

  private:
    enum class PulseEvent : uint8_t
    {
        QUIT,
        TIMER
    };

    void SendPulseEvent(PulseEvent pulse_event) noexcept;
    void ProcessPulseEvent(PulseEvent pulse_event) noexcept;
    void ProcessCleanup(const void* const owner) noexcept;
    void RunOnThread() noexcept;

    static int EndpointFdSelectCallback(select_context_t* ctp, int fd, unsigned flags, void* handle) noexcept;
    static int TimerPulseCallback(message_context_t* ctp, int code, unsigned flags, void* handle) noexcept;
    static int EventPulseCallback(message_context_t* ctp, int code, unsigned flags, void* handle) noexcept;

    void UnselectEndpoint(PosixEndpointEntry& endpoint) noexcept;
    void ProcessTimerQueue() noexcept;

    void SetupResourceManagerCallbacks() noexcept;
    score::cpp::expected_blank<score::os::Error> StartServer(ResourceManagerServer& server,
                                                    const QnxResourcePath& path) noexcept;
    void StopServer(ResourceManagerServer& server) noexcept;

    static ResourceManagerConnection& OcbToConnection(RESMGR_OCB_T* const ocb)
    {
        return *static_cast<ResourceManagerConnection*>(ocb);
    }

    static ResourceManagerServer& ResmgrHandleToServer(RESMGR_HANDLE_T* const handle)
    {
        return *static_cast<ResourceManagerServer*>(handle);
    }

    static ResourceManagerServer& OcbToServer(RESMGR_OCB_T* const ocb)
    {
        return *static_cast<ResourceManagerServer*>(ocb->attr);
    }

    static std::int32_t io_open(resmgr_context_t* const ctp,
                                io_open_t* const msg,
                                RESMGR_HANDLE_T* const handle,
                                void* const extra) noexcept;

    static std::int32_t io_close_ocb(resmgr_context_t* const ctp,
                                     void* const reserved,
                                     RESMGR_OCB_T* const ocb) noexcept;

    static std::int32_t io_write(resmgr_context_t* const ctp, io_write_t* const msg, RESMGR_OCB_T* const ocb) noexcept;
    static std::int32_t io_read(resmgr_context_t* const ctp, io_read_t* const msg, RESMGR_OCB_T* const ocb) noexcept;
    static std::int32_t io_notify(resmgr_context_t* const ctp,
                                  io_notify_t* const msg,
                                  RESMGR_OCB_T* const ocb) noexcept;

    score::cpp::pmr::memory_resource* const memory_resource_;
    OsResources os_resources_;

    bool quit_flag_;
    std::thread thread_;
    std::mutex thread_mutex_;
    std::condition_variable thread_condition_;

    detail::TimedCommandQueue timer_queue_;
    score::containers::intrusive_list<PosixEndpointEntry> posix_endpoint_list_;
    score::cpp::pmr::vector<std::uint8_t> posix_receive_buffer_;

    dispatch_t* dispatch_pointer_;
    dispatch_context_t* context_pointer_;
    std::int32_t side_channel_coid_;
    timer_t timer_id_;
    std::mutex attach_mutex_;

    resmgr_connect_funcs_t connect_funcs_{};
    resmgr_io_funcs_t io_funcs_{};
};

}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_QNX_DISPATCH_QNX_DISPATCH_ENGINE_H
