# sigslot - C++11 Signal/Slot library

Originally written by Sarah Thompson.

Various patches and fixes applied by Cat Nap Games:

To make this compile under Xcode 4.3 with Clang 3.0 I made some changes myself and also used some diffs published in the original project's Sourceforge forum.
I don't remember which ones though.

C++11-erization (and C++2x-erixation, and mini coroutine library) by Dave Cridland:

See example.cc and co_example.cc for some documentation and a walk-through example, or read the tests.

This is public domain; no copyright is claimed or asserted.

No warranty is implied or offered either.

## Tagging and version

Until recently, I'd say just use HEAD. But some people are really keen on tags, so I'll do some semantic version tagging on this.

## Promising, yet oddly vague and  sometimes outright misleading documentation

This library is a pure header library, and consists of four header files:

<sigslot/siglot.h>

This contains a sigslot::signal<T...> class, and a sigslot::has_slots class.

Signals can be connected to arbitrary functions, but in order to handle disconnect on lifetime termination, there's a "has_slots" base class to make it simpler.

Loosely, calling "emit(...)" on the signal will then call all the connected "slots", which are just arbitrary functions.

If a class is derived (publicly) from has_slots, you can pass in the instance of the class you want to control the lifetime. For calling a specific member directly, that's an easy decision; but if you pass in a lambda or some other arbitrary function, it might not be.

If there's nothing obvious to hand, something still needs to control the scope - leaving out the has_slots argument therefore returns you a (deliberately undocumented) placeholder class, which acts in lieu of a has_slots derived class of your choice.

<sigslot/tasklet.h>

This has a somewhat integrated coroutine library. Tasklets are coroutines, and like most coroutines they can be started, resumed, etc. There's no generator defined, just simple coroutines.

Tasklets expose co_await, so can be awaited by other coroutines. Signals can also be awaited upon, and will resolve to nothing (ie, void), or the single type, or a std::tuple of the types.

<sigslot/resume.h>

Coroutine resumption can be tricky, and is usually best integrated into some kind of event loop. Failure to do so will make it very hard to do anything that you couldn't do as well (or better!) without.

You can define your own resume function which will be called when a coroutine should be resumed, a trivial (and rather poor) example is at the beginning of the co_thread tests.

If you don't, then std::coroutine_handle<>::resume() will be called directly (which works for trivial cases, but not for anything useful).

<sigslot/cothread.h>

sigslot::co_thread is a convenient (but very simple) wrapper to run a non-coroutine in a std::jthread, but outwardly behave as a coroutine. Construct once, and it can be treated as a coroutine definition thereafter, and called multiple times.

This will not work with the built-in resumption, you'll need to implement *some* kind of event loop.
