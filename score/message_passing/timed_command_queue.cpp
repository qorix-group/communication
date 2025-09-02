#include "score/message_passing/timed_command_queue.h"

namespace score
{
namespace message_passing
{
namespace detail
{

void TimedCommandQueue::RegisterImmediateEntry(Entry& entry, QueuedCallback callback, const void* const owner) noexcept
{
    RegisterTimedEntry(entry, TimePoint{}, std::move(callback), owner);
}

void TimedCommandQueue::RegisterTimedEntry(Entry& entry,
                                           const TimePoint until,
                                           QueuedCallback callback,
                                           const void* const owner) noexcept
{
    entry.until_ = until;
    entry.owner_ = owner;
    entry.callback_ = std::move(callback);

    std::lock_guard<std::mutex> guard(mutex_);
    queue_.insert(std::find_if(queue_.begin(),
                               queue_.end(),
                               [until](const Entry& queue_entry) {
                                   return until < queue_entry.until_;
                               }),
                  entry);
}

TimedCommandQueue::TimePoint TimedCommandQueue::ProcessQueue(const TimePoint now) noexcept
{
    std::unique_lock<std::mutex> lock(mutex_);
    while (!queue_.empty())
    {
        Entry& queue_entry = queue_.front();
        const TimePoint until = queue_entry.until_;
        if (until > now)
        {
            return until;
        }
        queue_.pop_front();
        {
            QueuedCallback callback = std::move(queue_entry.callback_);
            lock.unlock();  // give the callback ability to reschedule itself
            callback(now);
        }
        lock.lock();
    }
    return TimePoint{};
}

void TimedCommandQueue::CleanUpOwner(const void* const owner) noexcept
{
    std::unique_lock<std::mutex> lock(mutex_);
    queue_.remove_and_dispose_if(
        [owner](auto& queue_entry) {
            return queue_entry.owner_ == owner;
        },
        [&lock](auto queue_entry) noexcept {
            lock.unlock();
            queue_entry->callback_ = QueuedCallback{};
            lock.lock();
        });
}

}  // namespace detail
}  // namespace message_passing
}  // namespace score
