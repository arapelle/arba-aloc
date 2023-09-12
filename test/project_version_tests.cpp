#include <arba/aloc/version.hpp>
#include <gtest/gtest.h>
#include <cstdlib>

TEST(project_version_tests, test_version_macros)
{
    ASSERT_EQ(ARBA_ALOC_VERSION_MAJOR, 0);
    ASSERT_EQ(ARBA_ALOC_VERSION_MINOR, 1);
    ASSERT_EQ(ARBA_ALOC_VERSION_PATCH, 0);
    ASSERT_STREQ(ARBA_ALOC_VERSION, "0.1.0");
}
