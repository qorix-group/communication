#include "score/message_passing/qnx_dispatch/qnx_resource_path.h"

#include <score/assert.hpp>
#include <score/utility.hpp>

namespace score
{
namespace message_passing
{
namespace detail
{

constexpr std::size_t QnxResourcePath::kMaxIdentifierLen;

QnxResourcePath::QnxResourcePath(const score::cpp::string_view identifier) noexcept
    : buffer_{GetQnxPrefix().cbegin(), GetQnxPrefix().cend()}
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION((identifier.size() > 0) && (identifier.size() <= kMaxIdentifierLen));
    auto identifier_begin = identifier.cbegin();
    if (*identifier_begin == '/')
    {
        ++identifier_begin;
    }
    score::cpp::ignore = buffer_.insert(buffer_.cend(), identifier_begin, identifier.cend());
    buffer_.push_back(0);
}

}  // namespace detail
}  // namespace message_passing
}  // namespace score
