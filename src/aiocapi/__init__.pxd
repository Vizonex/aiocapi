# from cpython.exc cimport PyErr_SetObject
# from cpython.object cimport PyObject

# cdef extern from "aiocapi_struct.h":
#     ctypedef enum FutureState:
#         # FUTURE_ERROR (not in CPython) is custom and it's a way to 
#         # detect failure.
#         FUTURE_ERROR = -1
#         FUTURE_STATE_PENDING = 0
#         FUTURE_STATE_CANCELLED = 1
#         FUTURE_STATE_FINISHED = 2
#     ctypedef int (*call_soon_cb)(void* user_data, object loop, object func, object arg, object ctx) except -1


# cdef extern from "aiocapi_cython.h":
#     int aiocapi_import() except -1
#     object Future_GetLoop(object self)
#     FutureState Future_GetState(object self) except FUTURE_ERROR
#     int Future_Done(object self) except -1
#     int Future_Cancel(object self, object message) except -1
#     int Future_CancelAdv(object self,  object message, void *user_data, call_soon_cb call_soon_fn) except -1
#     int Future_Cancelled(object self) except -1
#     object Future_Result(object self)
#     object Future_Exception(object self)
#     bint Future_HasException(object self) except -1
#     int Future_SetResultAdv(object self, object result, void *user_data, call_soon_cb call_soon_func) except -1
#     int Future_SetResult(object self, object result) except -1
#     int Future_SetExceptionAdv(object self, object exception, void *user_data, call_soon_cb call_soon_func) except -1
#     int Future_SetException(object self, object result) except -1
#     bint Future_CheckExact(object obj)
#     bint Future_Check(object obj)


# cdef extern from *:
#     # Helps users to build out different C-Extension classes.
#     # it doesn't have any other purposes beyond that.
#     # from aiocapi cimport Future
#     # cdef class MyFuture(Future): 
#     #    # if you understand the idea, it's just a speedup technique
#     #    ...

#     ctypedef class aiocapi.Future [object PyObject, check_size ignore]:
#         pass

#     ctypedef class aiocapi.Task [object PyObject, check_size ignore]:
#         pass