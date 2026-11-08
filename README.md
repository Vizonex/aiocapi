aiocapi
-------

A small yet simple recast hack to speedup low level asyncio 
modules that use C rather than pure python. It is used to
provide lower level libraries a CAPI that need to utilize asyncio.


This library also comes with a built-in and ready to go Cython
API to utilize, It's a bit different to multidict but still has it's ways of getting around known bottlenecks (like sending arguments and then having python later parse them), it can prove useful for projects such as winloop and uvloop to have tools like these readily avalible as they can get.

It's primary a library that is meant to prove to the CPython Development Team that people like us want these tools pretty badly as soon as it happens. I'll decommission later versions for backwards compatability depending on what criteria is met.

## History
When improving my very first pypi pacakge when I was still a beginner at coding. Winloop has had a historical number of people come in and out as well as different improvements and ideas. When those improvements started to run dry it only made sense to explore some other notatble issues. One of those that came up was lack of communications between asyncio objects. Originally winloop had a built-in Loop that was mostly made for cython to interpret but not meant to be exposed elsewhere but something just wasn't right and sometimes the Free-threaded mode would straight up Freeze when running my workflows, I knew then that something had to change and maybe a bit bigger. Using compiled asyncio objects rather than the pure python objects had it's own set of advantages with the only disadvantage possibily being if you didn't have a compield _asyncio.pyd or _asyncio.so file laying around somewhere since winloop takes advange of C-Extensions by default, it only made sense to give it just that and eliminate the pure python portions of it's code. My goal with this project is to possibly have multiple people step in and adopt the project including but not limited to rsloop, blazio and uvloop and the aiolibs team might also take intrest. 


## How does it work?

This libary is a lot simillarities to aiofastnet which takes advantage of dlls and openssl apis. This project takes advantage of skills pioneered from multidict and several other ideas and puts it into one place. Rather than being your simple ordinary python library it is
made for C/C++ and Cython based projects.

```cython
from aiocapi cimport Future, Future_CancelCb, import_aiocapi

import_aiocapi()

# This code may or may not compile but it is to demonstrate a real use-case.

# Hypothetically this will be our Cythonic EventLoop
cdef class Loop:
    cdef object faster_call_soon(self, object callback, tuple args, object context):
        pass

cdef int on_loop_call_soon(
    void* user_data,
    object loop,
    object func,
    object arg,
    object ctx
) except -1:
    (<Loop>loop).faster_call_soon(func, arg, ctx)
    return 0

cdef int future_cancel(object fut) except -1:
    # low-leveled callback with shortcut to a faster function call
    return Future_CancelCb(fut, "cancelled future", NULL, on_loop_call_soon)

# cdef class extension from external source.
cdef class FutureExtension(Future):
    pass
```

## Checklist of things needed before release

- 3.9
    - Futures [ ]
    - Tasks   [ ]
    - Tests   [ ]

- 3.10
    - Futures [ ]
    - Tasks   [ ]
    - Tests   [ ]

- 3.11
    - Futures [X]
    - Tasks   [ ]
    - Tests   [ ]

- 3.12 
    - Futures [ ]
    - Tasks   [ ]
    - Tests   [ ]

- 3.13
    - Futures [ ]
    - Tasks   [ ]
    - Tests   [ ]

- 3.14
    - Futures [ ]
    - Tasks   [ ]
    - Tests   [ ]

- 3.15
    - Futures [ ]
    - Tasks   [ ]
    - Tests   [ ]

- Cython [x]

