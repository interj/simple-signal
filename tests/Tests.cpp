#include "../signal.hpp"
#include <gtest/gtest.h>

struct SignalTests: public ::testing::Test
{
    Signal<void()> sig;
    struct Example
    {
        void exampleOp() {}
    };
};

void exampleFreeFunc() {}

TEST_F(SignalTests, disconnects)
{
    auto connection = sig.connect([]{});
    EXPECT_TRUE(connection.isConnected());
    connection.disconnect();
    EXPECT_FALSE(connection.isConnected());
};

TEST_F(SignalTests, releases)
{
    auto connection = sig.connect([]{});
    EXPECT_TRUE(connection.isConnected());
    auto movedAction = connection.release();
    EXPECT_FALSE(connection.isConnected());

    Connection newConnection = std::move(movedAction);
    EXPECT_TRUE(newConnection.isConnected());
};

TEST_F(SignalTests, simpleConnsAreMovable)
{
    auto simpleLambdaConn = sig.connect([]{});
    EXPECT_TRUE(std::is_move_assignable_v<decltype(simpleLambdaConn)> && std::is_move_constructible_v<decltype(simpleLambdaConn)>);
    auto freeFuncConn = sig.connect(exampleFreeFunc);
    EXPECT_TRUE(std::is_move_assignable_v<decltype(freeFuncConn)> && std::is_move_constructible_v<decltype(freeFuncConn)>);

    Example ex;
    auto memberFuncConn = sig.connect(std::bind(&Example::exampleOp,  &ex));
    EXPECT_FALSE(std::is_move_assignable_v<decltype(memberFuncConn)> && std::is_move_constructible_v<decltype(memberFuncConn)>);

    int localVar;
    auto captureLambdaConn = sig.connect([&localVar]{});
    EXPECT_FALSE(std::is_move_assignable_v<decltype(captureLambdaConn)> && std::is_move_constructible_v<decltype(captureLambdaConn)>);
};

TEST_F(SignalTests, callsAreIgnored)
{
    int timesCalled = 0;
    auto connection = sig.connect([&timesCalled]{timesCalled++;});
    sig();
    EXPECT_EQ(timesCalled, 1);
    auto movedAction = connection.release();
    sig();
    EXPECT_EQ(timesCalled, 1);

    Connection newConnection = std::move(movedAction);
    sig();
    EXPECT_EQ(timesCalled, 2);
    newConnection.disconnect();
    sig();
    EXPECT_EQ(timesCalled, 2);

};


