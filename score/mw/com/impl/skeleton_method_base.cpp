#include "score/mw/com/impl/skeleton_method_base.h"

namespace score::mw::com::impl
{
void SkeletonMethodBase::UpdateSkeletonReference(SkeletonBase& skeleton_base) noexcept
{
    skeleton_base_ = skeleton_base;
}
}  // namespace score::mw::com::impl
