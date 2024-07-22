//
// Created by dave on 28/03/2024.
//

#include <gtest/gtest.h>
#include <sigslot/sigslot.h>

template<typename ...Args>
class Sink : public sigslot::has_slots {
public:
    std::optional<std::tuple<Args...>> result;
    void slot(Args... args) {
        result.emplace(args...);
    }
    void reset() {
        result.reset();
    }
};

template<>
class Sink<void> : public sigslot::has_slots {
public:
    bool result = false;
    void slot() {
        result = true;
    }
    void reset() {
        result = false;
    }
};

TEST(Simple, test_bool) {
    Sink<bool> sink;
    EXPECT_FALSE(sink.result.has_value());
    sigslot::signal<bool> signal;
    signal.connect(&sink, &Sink<bool>::slot);
    signal(true);
    EXPECT_TRUE(sink.result.has_value());
    EXPECT_TRUE(std::get<0>(*sink.result));
    sink.reset();
    signal(false);
    EXPECT_TRUE(sink.result.has_value());
    EXPECT_FALSE(std::get<0>(*sink.result));
}


TEST(Simple, test_bool_disconnect) {
    sigslot::signal<bool> signal;
    signal(true);
    {
        Sink<bool> sink;
        EXPECT_FALSE(sink.result.has_value());
        sigslot::signal<bool> signal;
        signal.connect(&sink, &Sink<bool>::slot);
        signal(true);
        EXPECT_TRUE(sink.result.has_value());
        EXPECT_TRUE(std::get<0>(*sink.result));
    }
    signal(false);
}


TEST(Simple, test_bool_oneshot) {
    Sink<bool> sink;
    EXPECT_FALSE(sink.result.has_value());
    sigslot::signal<bool> signal;
    signal.connect(&sink, &Sink<bool>::slot, true);
    signal(true);
    EXPECT_TRUE(sink.result.has_value());
    EXPECT_TRUE(std::get<0>(*sink.result));
    sink.reset();
    signal(false);
    EXPECT_FALSE(sink.result.has_value());
}


TEST(Simple, test_void) {
    Sink<void> sink;
    EXPECT_FALSE(sink.result);
    sigslot::signal<> signal;
    signal.connect(&sink, &Sink<void>::slot);
    signal();
    EXPECT_TRUE(sink.result);
    sink.reset();
    signal();
    EXPECT_TRUE(sink.result);
}


TEST(Simple, test_void_oneshot) {
    Sink<void> sink;
    EXPECT_FALSE(sink.result);
    sigslot::signal<> signal;
    signal.connect(&sink, &Sink<void>::slot, true);
    signal();
    EXPECT_TRUE(sink.result);
    sink.reset();
    signal();
    EXPECT_FALSE(sink.result);
}


