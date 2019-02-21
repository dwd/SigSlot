#include <sigslot/sigslot.h>
#include <sigslot/tasklet.h>
#include <iostream>
#include <string>

/**
 * We'll use some global signals here.
 */
sigslot::signal<> tick;
sigslot::signal<int> tock;
sigslot::signal<int, std::string> splat;

/**
 * Our simple coroutine.
 *
 * All it's going to do is await the two signals - we won't do anything with it.
 */

sigslot::tasklet<int> coroutine_example() {
    std::cout << "C: Ready." << std::endl;
    /**
     * If you co_await a signal, execution stops and control moves to the caller.
     */
    co_await tick;
    /**
     * And now the signal must have been triggered.
     *
     * Awaiting a signal is inherently one-shot, if the signal is triggered twice,
     * we won't know about it.
     */
    std::cout << "C: Got a tick." << std::endl;
    /**
     * For signals with a single argument, the argument gets returned by the
     * co_await when it completes:
     */
    auto foo = co_await tock;
    std::cout << "C: Got a tock of " << foo << std::endl;
    /**
     * Signals that have multiple arguments also work, but it passes back a std::tuple
     * in this case. This is relatively easy to unwrap with a std::tie, though it would
     * be simpler with C++17's Structured Bindings.
     */
    int x;
    std::string s;
    std::tie(x, s) = co_await splat;
    std::cout << "C: Got a splat of " << x << ", " << s << std::endl;
    co_return foo;
}

sigslot::tasklet<int> wrapping_coroutine() {
    auto task = coroutine_example();
    std::cout << "W: Starting an inner coroutine." << std::endl;
    task.start();
    std::cout << "W: Waiting" << std::endl;
    auto foo = co_await task;
    std::cout << "W: Inner coroutine completed with " << foo << std::endl;
    co_return foo;
}

sigslot::tasklet<void> throws_exception() {
    std::cout << "I shall throw an exception:" << std::endl;
    throw std::runtime_error("This is an exception.");
    co_return; // This is unreachable, but needed since otherwise it's not a coroutine!
}

sigslot::tasklet<bool> catch_exceptions() {
    try {
        co_await throws_exception();
        co_return false;
    } catch(std::runtime_error & e) {
        std::cout << "Caught: " << e.what() << std::endl;
        co_return true;
    }
}

int main(int argc, char *argv[]) {
    try {
        /**
         * First with the coroutine awaiting:
         */
        std::cout << "M: Executing coroutine." << std::endl;
        auto c = wrapping_coroutine();
        c.start(); // Start the tasklet. It'll execute until it needs to await a signal, then stop and return.
        // Tasklets automatically start if you get() or co_await them, but
        // if they then suspend you'll get a runtime error.
        std::cout << "M: Coroutine started, now running: " << c.running() << std::endl;
        std::cout << "M: Tick:" << std::endl;
        tick(); // When we emit the signal, it'll start executing the coroutine again. Again, it'll stop when it awaits the next signal.
        std::cout << "M: Tock(42):" << std::endl;
        tock(42);
        std::cout << "M: Splat(17, \"Gerbils\")" << std::endl;
        splat(17, "Gerbils");
        std::cout << "M: Answer is " << c.get() << std::endl;
        /**
         * If we sent the second signal before the first, the coroutine would wait forever.
         * This is because it wouldn't fire when the coroutine is suspended in co_await.
         */
         auto ex = catch_exceptions();
         ex.start();
         if (ex.get()) {
             std::cout << "Caught the exception properly" << std::endl;
         } else {
             throw std::runtime_error("Didn't catch exception!");
         }
         try {
             auto ex1 = throws_exception();
             std::cout << "Here we go." << std::endl;
             ex1.get();
             throw std::runtime_error("Didn't catch exception!");
         } catch (std::runtime_error & e) {
             std::cout << "Expected exception caught: " << e.what()  << std::endl;
         }
    } catch (std::exception const & e) {
        std::cerr << e.what() << std::endl;
        throw;
    }
}