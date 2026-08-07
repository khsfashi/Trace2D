#include <trace2d/core/Version.hpp>

#include <gtest/gtest.h>

TEST(VersionTests, ReportsProjectVersion)
{
    EXPECT_EQ(trace2d::core::Version(), "0.1.0");
}
