//
// Created by dave on 22/07/2024.
//

#ifndef SIGSLOT_RESUME_H
#define SIGSLOT_RESUME_H

#include <coroutine>

namespace sigslot {
    template<typename T>
    struct resumer {
        static void resume(T coro) {
            coro.resume();
        }
    };
}

#endif //SIGSLOT_RESUME_H
