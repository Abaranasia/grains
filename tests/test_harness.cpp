#include <catch2/catch_test_macros.hpp>

TEST_CASE("harness: test runner is wired to ctest", "[harness]")
{
    REQUIRE(1 + 1 == 2);
}
