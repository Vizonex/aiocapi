#ifndef __AIO_311_H__
#define __AIO_311_H__

#include "Python.h"

/* 3.11 Guard */
#if (PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION == 11)
    #define AIO311
#endif 


#ifdef AIO311

#ifdef __cplusplus
extern "C" {
#endif

#include "../aiocapi_struct.h"
#include "module_state.h"


/* Object Definitions, All Fields are Always copied so that multiple
problems are prevented, Internals are not copied and that is by design, 
Python has direct access to __await__ and Cython knows how to handle those 
parts. */

typedef enum {
    STATE_PENDING,
    STATE_CANCELLED,
    STATE_FINISHED
} fut_state;

#define FutureObj_HEAD(prefix)                                              \
    PyObject_HEAD                                                           \
    PyObject *prefix##_loop;                                                \
    PyObject *prefix##_callback0;                                           \
    PyObject *prefix##_context0;                                            \
    PyObject *prefix##_callbacks;                                           \
    PyObject *prefix##_exception;                                           \
    PyObject *prefix##_exception_tb;                                        \
    PyObject *prefix##_result;                                              \
    PyObject *prefix##_source_tb;                                           \
    PyObject *prefix##_cancel_msg;                                          \
    fut_state prefix##_state;                                               \
    int prefix##_log_tb;                                                    \
    int prefix##_blocking;                                                  \
    PyObject *dict;                                                         \
    PyObject *prefix##_weakreflist;                                         \
    PyObject *prefix##_cancelled_exc;



typedef struct {
    FutureObj_HEAD(fut)
} FutureObj;

typedef struct {
    FutureObj_HEAD(task)
    PyObject *task_fut_waiter;
    PyObject *task_coro;
    PyObject *task_name;
    PyObject *task_context;
    int task_must_cancel;
    int task_log_destroy_pending;
    int task_num_cancels_requested;
} TaskObj;


static inline int
future_is_alive(FutureObj *fut)
{
    return fut->fut_loop != NULL;
}

static inline int
future_ensure_alive(FutureObj *fut)
{
    if (!future_is_alive(fut)) {
        PyErr_SetString(PyExc_RuntimeError,
                        "Future object is not initialized.");
        return -1;
    }
    return 0;
}

static int
call_soon(mod_state* state, PyObject *loop, PyObject *func, PyObject *arg, PyObject *ctx)
{
    PyObject *handle;
    PyObject *stack[3];
    Py_ssize_t nargs;

    if (ctx == NULL) {
        handle = PyObject_CallMethod(loop, "call_soon", "(OO)", func, arg);
    }
    else {
        /* Use FASTCALL to pass a keyword-only argument to call_soon */
        PyObject *callable = PyObject_GetAttrString(loop, "call_soon");
        if (callable == NULL) {
            return -1;
        }

        /* All refs in 'stack' are borrowed. */
        nargs = 1;
        stack[0] = func;
        if (arg != NULL) {
            stack[1] = arg;
            nargs++;
        }
        stack[nargs] = (PyObject *)ctx;

        handle = PyObject_Vectorcall(callable, stack, nargs, state->context_kwname);
        Py_DECREF(callable);
    }

    if (handle == NULL) {
        return -1;
    }
    Py_DECREF(handle);
    return 0;
}

static int default_call_soon(void* user_data, PyObject *loop, PyObject *func, PyObject *arg, PyObject *ctx){
    return call_soon((mod_state*)user_data, loop, func, arg, ctx);
}

static int
future_schedule_callbacks(FutureObj *fut, void* user_data, call_soon_cb call_soon_fn)
{
    Py_ssize_t len;
    Py_ssize_t i;

    if (fut->fut_callback0 != NULL) {
        /* There's a 1st callback */
        
        /* this was modified for performance improvements */
        int ret = call_soon_fn(
            user_data, fut->fut_loop, fut->fut_callback0,
            (PyObject *)fut, fut->fut_context0);

        Py_CLEAR(fut->fut_callback0);
        Py_CLEAR(fut->fut_context0);
        if (ret) {
            /* If an error occurs in pure-Python implementation,
               all callbacks are cleared. */
            Py_CLEAR(fut->fut_callbacks);
            return ret;
        }

        /* we called the first callback, now try calling
           callbacks from the 'fut_callbacks' list. */
    }

    if (fut->fut_callbacks == NULL) {
        /* No more callbacks, return. */
        return 0;
    }

    len = PyList_GET_SIZE(fut->fut_callbacks);
    if (len == 0) {
        /* The list of callbacks was empty; clear it and return. */
        Py_CLEAR(fut->fut_callbacks);
        return 0;
    }

    for (i = 0; i < len; i++) {
        PyObject *cb_tup = PyList_GET_ITEM(fut->fut_callbacks, i);
        PyObject *cb = PyTuple_GET_ITEM(cb_tup, 0);
        PyObject *ctx = PyTuple_GET_ITEM(cb_tup, 1);

        if (call_soon_fn(user_data, fut->fut_loop, cb, (PyObject *)fut, ctx)) {
            /* If an error occurs in pure-Python implementation,
               all callbacks are cleared. */
            Py_CLEAR(fut->fut_callbacks);
            return -1;
        }
    }

    Py_CLEAR(fut->fut_callbacks);
    return 0;
}

static PyObject *
create_cancelled_error(mod_state* state, FutureObj *fut)
{
    PyObject *exc;
    if (fut->fut_cancelled_exc != NULL) {
        /* transfer ownership */
        exc = fut->fut_cancelled_exc;
        fut->fut_cancelled_exc = NULL;
        return exc;
    }
    PyObject *msg = fut->fut_cancel_msg;
    if (msg == NULL || msg == Py_None) {
        exc = PyObject_CallNoArgs(state->asyncio_CancelledError);
    } else {
        exc = PyObject_CallOneArg(state->asyncio_CancelledError, msg);
    }
    return exc;
}

static void
future_set_cancelled_error(mod_state* state, FutureObj *fut)
{
    PyObject *exc = create_cancelled_error(state, fut);
    if (exc == NULL) {
        return;
    }
    PyErr_SetObject(state->asyncio_CancelledError, exc);
    Py_DECREF(exc);
}

static int
future_get_result(mod_state* state, FutureObj *fut, PyObject **result)
{
    if (fut->fut_state == STATE_CANCELLED) {
        future_set_cancelled_error(state, fut);
        return -1;
    }

    if (fut->fut_state != STATE_FINISHED) {
        PyErr_SetString(state->asyncio_InvalidStateError, "Result is not set.");
        return -1;
    }

    fut->fut_log_tb = 0;
    if (fut->fut_exception != NULL) {
        PyObject *tb = fut->fut_exception_tb;
        if (tb == NULL) {
            tb = Py_None;
        }
        if (PyException_SetTraceback(fut->fut_exception, tb) < 0) {
            return -1;
        }
        Py_INCREF(fut->fut_exception);
        *result = fut->fut_exception;
        Py_CLEAR(fut->fut_exception_tb);
        return 1;
    }

    Py_INCREF(fut->fut_result);
    *result = fut->fut_result;
    return 0;
}

static int
future_set_result(mod_state* state, FutureObj *fut, PyObject *res, void* user_data, call_soon_cb call_soon_fn)
{
    if (future_ensure_alive(fut)) {
        return -1;
    }

    if (fut->fut_state != STATE_PENDING) {
        PyErr_SetString(state->asyncio_InvalidStateError, "invalid state");
        return -1;
    }

    assert(!fut->fut_result);
    Py_INCREF(res);
    fut->fut_result = res;
    fut->fut_state = STATE_FINISHED;

    if (future_schedule_callbacks(fut, user_data, call_soon_fn) == -1) {
        return -1;
    }
    return 0;
}

static int
future_set_exception(mod_state* state, FutureObj *fut, PyObject *exc, void* user_data, call_soon_cb call_soon_fn)
{
    PyObject *exc_val = NULL;

    if (fut->fut_state != STATE_PENDING) {
        PyErr_SetString(state->asyncio_InvalidStateError, "invalid state");
        return -1;
    }

    if (PyExceptionClass_Check(exc)) {
        exc_val = PyObject_CallNoArgs(exc);
        if (exc_val == NULL) {
            return -1;
        }
        if (fut->fut_state != STATE_PENDING) {
            Py_DECREF(exc_val);
            PyErr_SetString(state->asyncio_InvalidStateError, "invalid state");
            return -1;
        }
    }
    else {
        exc_val = exc;
        Py_INCREF(exc_val);
    }
    if (!PyExceptionInstance_Check(exc_val)) {
        Py_DECREF(exc_val);
        PyErr_SetString(PyExc_TypeError, "invalid exception object");
        return -1;
    }
    if (Py_IS_TYPE(exc_val, (PyTypeObject *)PyExc_StopIteration)) {
        Py_DECREF(exc_val);
        PyErr_SetString(PyExc_TypeError,
                        "StopIteration interacts badly with generators "
                        "and cannot be raised into a Future");
        return -1;
    }

    assert(!fut->fut_exception);
    assert(!fut->fut_exception_tb);
    fut->fut_exception = exc_val;
    fut->fut_exception_tb = PyException_GetTraceback(exc_val);
    fut->fut_state = STATE_FINISHED;

    if (future_schedule_callbacks(fut, user_data, call_soon_fn) == -1) {
        return -1;
    }

    fut->fut_log_tb = 1;
    return 0;

}


/* C-API */

// static PyObject* 
// Future_New(void* _state, PyObject* loop){
//     mod_state* state = (mod_state*)_state;
//     if (loop == NULL){
//         return PyObject_CallNoArgs((PyObject*)state->FutureType);
//     }
//     /* It can be significantly easier to hand this off as a Python call */
//     return PyObject_CallOneArg((PyObject*)state->FutureType, loop);
// }

static PyObject* 
Future_GetLoop(void* _state, PyObject* self){
    FutureObj* fut;
    __FUTURE_VALIDATION_CHECK(self, _state, NULL)
    if (future_is_alive((FutureObj*)self)){
        Py_RETURN_NONE;
    }
    fut = (FutureObj*)self;
    Py_INCREF(fut->fut_loop);
    return fut->fut_loop;
}

static FutureState 
Future_GetState(void* _state, PyObject* self){
    __FUTURE_VALIDATION_CHECK(self, _state, FUTURE_ERROR)
    if (future_ensure_alive((FutureObj*)self) < 0){
        return FUTURE_ERROR;
    }
    return (FutureState)(((FutureObj*)self)->fut_state);
};

static int 
Future_Done(void* _state, PyObject* self){
    FutureObj* fut;
    __FUTURE_VALIDATION_CHECK(self, _state, -1)
    fut = (FutureObj*)self;
    /* 0 or 1 */
    return (!future_is_alive(fut) || (fut->fut_state == STATE_PENDING));
}





static int 
Future_CancelCb(void *_state, PyObject *self, PyObject *message, void* user_data, call_soon_cb loop_call_soon){
    FutureObj* fut;
    __FUTURE_VALIDATION_CHECK(self, _state, -1)
    fut = (FutureObj*)self;
    fut->fut_log_tb = 0;

    if (fut->fut_state != STATE_PENDING) {
        return 0;
    }

    fut->fut_state = STATE_CANCELLED;
    Py_XINCREF(message);
    Py_XSETREF(fut->fut_cancel_msg, message);
    if (future_schedule_callbacks(fut, user_data, loop_call_soon) < 0) {
        return -1;
    }
    return 0;
}

/* The not-so advanced version of cancellation 
 * WARNING: This Falls back to a worse version of what CPython has 
 * As were working with The Intentionally Exposed C-API on different scopes. */
static int 
Future_Cancel(void *_state, PyObject *self, PyObject *message){
    return Future_CancelCb(_state, self, message, _state, default_call_soon);
}

static int 
Future_Cancelled(void* _state, PyObject* self){
    FutureObj* fut;
    __FUTURE_VALIDATION_CHECK(self, _state, -1)
    fut = (FutureObj*)self;
    return (future_is_alive(fut) && fut->fut_state == STATE_CANCELLED);
}

static PyObject*
Future_Result(void* _state, PyObject* self){
    FutureObj* fut;
    PyObject* result;
    __FUTURE_VALIDATION_CHECK(self, _state, NULL)
    fut = (FutureObj*)self;

    int res = future_get_result((mod_state*)_state, fut, &result);
    if (res == -1) {
        return NULL;
    }

    if (res == 0) {
        return result;
    }

    assert(res == 1);

    PyErr_SetObject(PyExceptionInstance_Class(result), result);
    Py_DECREF(result);
    return NULL;
}

static PyObject* 
Future_Exception(void* _state, PyObject* self){
    FutureObj* fut;
    mod_state* state = (mod_state*)_state;
    __FUTURE_VALIDATION_CHECK(self, _state, NULL)
    fut = (FutureObj*)self;

    if (!future_is_alive(fut)) {
        PyErr_SetString(state->asyncio_InvalidStateError,
                        "Future object is not initialized.");
        return NULL;
    }

    if (fut->fut_state == STATE_CANCELLED) {
        future_set_cancelled_error(state, fut);
        return NULL;
    }

    if (fut->fut_state != STATE_FINISHED) {
        PyErr_SetString(state->asyncio_InvalidStateError, "Exception is not set.");
        return NULL;
    }

    if (fut->fut_exception != NULL) {
        fut->fut_log_tb = 0;
        Py_INCREF(fut->fut_exception);
        return fut->fut_exception;
    }
    Py_RETURN_NONE;
}

/* Faster check than simply calling Future_Exception() or _asyncio.Future.exception() */
int Future_HasException(void* _state, PyObject* self){
    FutureObj* fut;
    __FUTURE_VALIDATION_CHECK(self, _state, -1)
    fut = (FutureObj*)self;

    if (future_ensure_alive(fut) < 0){
        return -1;
    }
    return (fut->fut_exception != NULL);
}

static int Future_SetResultCb(
    void* _state, PyObject* self, PyObject* result, void* user_data, call_soon_cb call_soon_func
){
    FutureObj* fut;
    __FUTURE_VALIDATION_CHECK(self, _state, -1)
    fut = (FutureObj*)self;

    if (future_ensure_alive(fut) < 0){
        return -1;
    }
    return future_set_result((mod_state*)_state, fut, result, user_data, call_soon_func);
}

static int 
Future_SetResult(void *_state, PyObject *self, PyObject *result){
    return Future_SetResultCb(_state, self, result, _state, default_call_soon);
}


static int Future_SetExceptionCb(
    void* _state, PyObject* self, PyObject* exception, void* user_data, call_soon_cb call_soon_func
){
    FutureObj* fut;
    __FUTURE_VALIDATION_CHECK(self, _state, -1)
    fut = (FutureObj*)self;

    if (future_ensure_alive(fut) < 0){
        return -1;
    }
    return future_set_exception((mod_state*)_state, fut, exception, user_data, call_soon_func);
}

static int 
Future_SetException(void *_state, PyObject *self, PyObject *result){
    return Future_SetExceptionCb(_state, self, result, _state, default_call_soon);
}




#ifdef __cplusplus
}
#endif

#endif /* AIO311 */
#endif /* __AIO_311_H__ */
