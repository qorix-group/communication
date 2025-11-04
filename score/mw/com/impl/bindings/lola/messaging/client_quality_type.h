#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGING_CLIENT_QUALITY_TYPE_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGING_CLIENT_QUALITY_TYPE_H

#include <cstdint>

namespace score::mw::com::impl::lola
{
enum class ClientQualityType : std::uint8_t
{
    kASIL_QM,
    kASIL_QMfromB,
    kASIL_B,
};
}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_MESSAGING_CLIENT_QUALITY_TYPE_H
