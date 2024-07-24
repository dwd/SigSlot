//
// Created by dave on 22/07/2024.
//

#ifndef SIGSLOT_RESUME_H
#define SIGSLOT_RESUME_H

#ifndef SIGSLOT_NO_COROUTINES
#include <coroutine>

namespace sigslot {
    namespace coroutines {
        struct sentinel {};
    }
    coroutines::sentinel resume(...);
}
#endif

#endif //SIGSLOT_RESUME_H