from cpython.object cimport PyObject

cdef extern from *:
    # Helps users to build out different C-Extension classes.
    # it doesn't have any other purposes beyond that.
    # from aiocapi cimport Future
    # cdef class MyFuture(Future): 
    #    # if you understand the idea, it's just a speedup technique
    #    ...

    ctypedef class aiocapi.Future [object PyObject, check_size ignore]:
        pass

    ctypedef class aiocapi.Task [object PyObject, check_size ignore]:
        pass

cdef extern from "aiocapi_struct.h":
    ctypedef enum FutureState:
        FUTURE_ERROR = -1
        FUTURE_STATE_PENDING = 0
        FUTURE_STATE_CANCELLED = 1
        FUTURE_STATE_FINISHED = 2

    ctypedef int (*aiocapi_call_soon_cb)(
        void* user_data,
        object loop,
        object func,
        object arg, 
        object ctx
    )

    ctypedef struct AioCAPI:
        int api_version
        void* state
        # NOTE: Some things are left out on purpose
        # EXAMPLE: PyTypeObject related pointers.

        object (*Future_GetLoop)(void* state, object self)
        FutureState (*Future_GetState)(void* state, object self)
        int (*Future_Done)(void* state, object self)
        int (*Future_CancelCb)(
            void* state, 
            object self, 
            PyObject* message, 
            void* user_data, 
            aiocapi_call_soon_cb call_soon_func
        )
        int (*Future_Cancel)(void* state, object self, PyObject* message)
        int (*Future_Cancelled)(void* state, object self)
        object (*Future_Result)(void* state, object self)
        object (*Future_Exception)(void* state, object self)
        int (*Future_HasException)(void* state, object self)
        int (*Future_SetResultCb)(void* state, object self, object result, void* user_data, aiocapi_call_soon_cb call_soon_func)
        int (*Future_SetResult)(void* state, object self, object result)
        int (*Future_SetExceptionCb)(void* state, object self, object exception, void* user_data, aiocapi_call_soon_cb call_soon_func)
        int (*Future_SetException)(void* state, object self, object exception)



cdef extern from "aiocapi.h":
    """
static AioCAPI* Pyx_AioAPI = NULL;
    """
    # global capsule simillar to PyDateTimeAPI 
    # naming as Pyx_AioAPI because it's Cython
    # an advantage is that it doesn't underscore
    # the naming so that Cython doesn't break during
    # compilation. 
    AioCAPI* AioAPI "Pyx_AioAPI"
    AioCAPI* AioCAPI_GetCAPI() except NULL
    int _Future_CheckExact "Future_CheckExact"(AioCAPI* capi, object obj)
    int _Future_Check "Future_Check"(AioCAPI* capi, object obj)



cdef inline int import_aiocapi() except -1:
    """imports unoffical asyncio c-api."""
    AioAPI = AioCAPI_GetCAPI()
    return 0


cdef inline object Future_GetLoop(object self):
    """gets underlying future's event-loop"""
    return AioAPI.Future_GetLoop(AioAPI.state, self)


cdef inline FutureState Future_GetState(object self) except FUTURE_ERROR:
    return AioAPI.Future_GetState(AioAPI.state, self)


cdef inline int Future_Done(object self) except -1:
    return AioAPI.Future_Done(AioAPI.state, self)


cdef inline int Future_Cancel(object self, object message) except -1:
    return AioAPI.Future_Cancel(AioAPI.state, self, message)


cdef inline int Future_CancelCb(
    object self, 
    object message, 
    void* user_data, 
    aiocapi_call_soon_cb call_soon_fn
) except -1:
    return AioAPI.Future_CancelCb(AioAPI.state, self, message, user_data, call_soon_fn)


cdef inline int Future_Cancelled(object self) except -1:
    return AioAPI.Future_Cancelled(AioAPI.state, self)


cdef inline object Future_Result(object self):
    return AioAPI.Future_Result(AioAPI.state, self)


cdef inline object Future_Exception(object self):
    return AioAPI.Future_Exception(AioAPI.state, self)


cdef inline int Future_HasException(object self) except -1:
    return AioAPI.Future_HasException(AioAPI.state, self)


cdef inline int Future_SetResultCb(
    object self, 
    object result, 
    void* user_data, 
    aiocapi_call_soon_cb call_soon_func
) except -1:
    return AioAPI.Future_SetResultCb(
        AioAPI.state, 
        self,
        result, 
        user_data, 
        call_soon_func
    )

cdef inline int Future_SetResult(object self, object result) except -1:
    return AioAPI.Future_SetResult(AioAPI.state, self, result)

cdef inline int Future_SetExceptionCb(
    object self,
    object exception,
    void* user_data,
    aiocapi_call_soon_cb call_soon_func
) except -1:
    return AioAPI.Future_SetExceptionCb(AioAPI.state, self, exception, user_data, call_soon_func)

cdef inline int Future_SetException(object self, object result) except -1:
    return AioAPI.Future_SetException(AioAPI.state, self, result)

cdef inline bint Future_Check(object obj) except -1:
    return _Future_Check(AioAPI, obj)

cdef inline bint Future_CheckExact(object obj) except -1:
    return _Future_CheckExact(AioAPI, obj)

cdef inline bint Future_HasException(object obj) except -1:
    return AioAPI.Future_HasException(AioAPI.state, obj)
