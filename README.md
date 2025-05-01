# sigslot - C++11 Signal/Slot library

Originally written by Sarah Thompson.

Various patches and fixes applied by Cat Nap Games:

To make this compile under Xcode 4.3 with Clang 3.0 I made some changes myself and also used some diffs published in the original project's Sourceforge forum.
I don't remember which ones though.

C++11-erization (and C++2x-erixation) by Dave Cridland:

See example.cc for some documentation and a walk-through example, or read the tests.

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
