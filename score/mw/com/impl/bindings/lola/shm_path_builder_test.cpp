/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#include "score/mw/com/impl/bindings/lola/shm_path_builder.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace score::mw::com::impl::lola
{
namespace
{

constexpr std::uint16_t kServiceId{4660U};

class ShmPathBuilderTestFixture
    : public ::testing::TestWithParam<std::tuple<QualityType, LolaServiceInstanceId::InstanceId, std::string>>
{
  public:
    static constexpr LolaServiceInstanceId::InstanceId kInstanceId{43981U};

  protected:
    std::string build_path(const std::string_view type,
                           const std::string_view expected_sub_path,
                           const QualityType quality_type = QualityType::kASIL_QM) const
    {
        std::ostringstream oss;
        oss << kBasePath << type << expected_sub_path << (quality_type == QualityType::kASIL_B ? kASILBTag : "");
        return oss.str();
    }

    ShmPathBuilder builder_{kServiceId};

    static constexpr auto kBasePath{"lola-"};
    static constexpr auto kDataTag{"data-"};
    static constexpr auto kControlTag{"ctl-"};
    static constexpr auto kASILBTag{"-b"};
};

TEST_P(ShmPathBuilderTestFixture, TestBuildingControlChannelShmNamePath)
{
    const auto [quality_type, instance_id, expected_sub_path] = GetParam();
    const auto expected_path = "/" + build_path(kControlTag, expected_sub_path, quality_type);

    const auto actual_path = builder_.GetControlChannelShmName(instance_id, quality_type);

    EXPECT_EQ(expected_path, actual_path);
}

TEST_P(ShmPathBuilderTestFixture, TestBuildingDataChannelShmNamePath)
{
    const auto [quality_type, instance_id, expected_sub_path] = GetParam();
    const auto expected_path = "/" + build_path(kDataTag, expected_sub_path);

    const auto actual_path = builder_.GetDataChannelShmName(instance_id);

    EXPECT_EQ(expected_path, actual_path);
}

INSTANTIATE_TEST_SUITE_P(
    ShmPathBuilderTests,
    ShmPathBuilderTestFixture,
    ::testing::Values(
        std::make_tuple(QualityType::kASIL_QM, LolaServiceInstanceId::InstanceId{1}, "0000000000004660-00001"),
        std::make_tuple(QualityType::kASIL_B, LolaServiceInstanceId::InstanceId{1}, "0000000000004660-00001"),
        std::make_tuple(QualityType::kASIL_QM, ShmPathBuilderTestFixture::kInstanceId, "0000000000004660-43981"),
        std::make_tuple(QualityType::kASIL_B, ShmPathBuilderTestFixture::kInstanceId, "0000000000004660-43981")));

}  // namespace
}  // namespace score::mw::com::impl::lola
