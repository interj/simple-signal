#include "../signal.hpp"
#include <gtest/gtest.h>

struct SignalTests: public ::testing::Test
{
    Signal<void()> sig;
};

TEST_F(SignalTests, Disconnects)
{
    auto connection = sig.connect([]{});
    connection.disconnect();
};
