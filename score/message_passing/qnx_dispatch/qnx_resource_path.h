#ifndef SCORE_LIB_MESSAGE_PASSING_QNX_DISPATCH_QNX_RESOURCE_PATH_H
#define SCORE_LIB_MESSAGE_PASSING_QNX_DISPATCH_QNX_RESOURCE_PATH_H

#include <score/static_vector.hpp>
#include <score/string_view.hpp>

namespace score
{
namespace message_passing
{
namespace detail
{

inline constexpr score::cpp::string_view GetQnxPrefix() noexcept
{
    using score::cpp::literals::operator""_sv;
    return "/bmw/message_passing/"_sv;
}

class QnxResourcePath
{
  public:
    explicit QnxResourcePath(const score::cpp::string_view identifier) noexcept;

    const char* c_str() const noexcept
    {
        return buffer_.data();
    }

    static constexpr std::size_t kMaxIdentifierLen = 256U;

  private:
    score::cpp::static_vector<char, GetQnxPrefix().size() + kMaxIdentifierLen + 1U> buffer_;
};

}  // namespace detail
}  // namespace message_passing
}  // namespace score

#endif  // SCORE_LIB_MESSAGE_PASSING_QNX_DISPATCH_QNX_RESOURCE_PATH_H
