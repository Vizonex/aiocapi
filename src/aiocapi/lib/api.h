#ifndef __AIOAPI_H__
#define __AIOAPI_H__

#include <Python.h>
#include "../asyncio_types.h"
#include "module_state.h"
#include "pythoncapi_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _Py_CRITICAL_SECTION_ASSERT_OBJECT_LOCKED
    /* WARNING in freethreading mode this lock does nothing!!! */
    #ifdef Py_GIL_DISABLED
        /* When you play stupid games, you win stupid prizes. */
        #warning "_Py_CRITICAL_SECTION_ASSERT_OBJECT_LOCKED has not been implemented bad things can happen if your not careful."
    #endif 
    #define _Py_CRITICAL_SECTION_ASSERT_OBJECT_LOCKED(obj)
#endif

static inline int
future_is_alive(FutureObj *fut)
{
    return fut->fut_loop != NULL;
}


static inline int 
future_ensure_alive(FutureObj* fut){
    if (!future_is_alive(fut)) {
        PyErr_SetString(PyExc_RuntimeError,
                        "Future object is not initialized.");
        return -1;
    }
    return 0;
}

/* safe recast mechanism this is for when state is sent as a void* type */
#define ms_cast(state) ((mod_state*)state)

/* 
Modified from _asynciomodule.c because we use a module state to 
control everything fail object is applied incase NULL or -1 is wanted. 
*/
#define ENSURE_FUTURE_ALIVE(fut, state, fail)                       \
    do {                                                            \
        assert(                                                     \
            Future_Check(ms_cast(state), fut) ||             \
            Task_Check(ms_cast(state), fut)                  \
        );                                                          \
        if (future_ensure_alive((FutureObj*)fut)) {                 \
            return fail;                                            \
        }                                                           \
    } while(0);

/* implementation of asyncio.futures._get_loop as well as CPython's 
adaptation */
static PyObject* 
GetLoop(void* state, PyObject* fut){
    PyObject* getloop;
    if (
        Future_CheckExact(ms_cast(state), fut) || 
        Task_CheckExact(ms_cast(state), fut)
    ) {
        PyObject *loop = ((FutureObj *)fut)->fut_loop;
        return Py_NewRef(loop);
    }
    if (PyObject_GetOptionalAttr(fut, ms_cast(state)->intern__get_loop, &getloop) < 0) {
        return NULL;
    }
    if (getloop != NULL) {
        PyObject *res = PyObject_CallNoArgs(getloop);
        Py_DECREF(getloop);
        return res;
    }
    return PyObject_GetAttr(fut, ms_cast(state)->intern___loop);
}


/* call_soon differs depending on version due to functional differences 
3.9, 3.10 are held back for that reason. */

#ifdef CPYTHON_ASYNCIO_39 || CPYTHON_ASYNCIO_310
static int
call_soon(void *state, PyObject* loop, PyObject *func, PyObject *arg, PyObject *ctx)
{
    PyObject *handle;
    PyObject *stack[3];
    Py_ssize_t nargs;

    if (ctx == NULL) {
        handle = PyObject_CallMethodObjArgs(
            loop, ms_cast(state)->intern__call_soon, func, arg, NULL);
    }
    else {
        /* Use FASTCALL to pass a keyword-only argument to call_soon */

        PyObject *callable = PyObject_GetAttr(loop, ms_cast(state)->intern__call_soon);
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

        handle = PyObject_Vectorcall(callable, stack, nargs, ms_cast(state)->context_kwname);
        Py_DECREF(callable);
    }

    if (handle == NULL) {
        return -1;
    }
    Py_DECREF(handle);
    return 0;
}

#else

static int
call_soon(void *state, PyObject *loop, PyObject *func, PyObject *arg,
          PyObject *ctx)
{
    PyObject *handle;

    if (ctx == NULL) {
        PyObject *stack[] = {loop, func, arg};
        size_t nargsf = 3 | PY_VECTORCALL_ARGUMENTS_OFFSET;
        handle = PyObject_VectorcallMethod(ms_cast(state)->intern__call_soon, stack, nargsf, NULL);
    }
    else {
        /* All refs in 'stack' are borrowed. */
        PyObject *stack[4];
        size_t nargs = 2;
        stack[0] = loop;
        stack[1] = func;
        if (arg != NULL) {
            stack[2] = arg;
            nargs++;
        }
        stack[nargs] = (PyObject *)ctx;
        size_t nargsf = nargs | PY_VECTORCALL_ARGUMENTS_OFFSET;
        handle = PyObject_VectorcallMethod(ms_cast(state)->intern__call_soon, stack, nargsf,
                                           ms_cast(state)->context_kwname);
    }
    if (handle == NULL) {
        return -1;
    }
    Py_DECREF(handle);
    return 0;
}
#endif /* call_soon */



/* future_schedule_callbacks is extremely important to have accross various 
different versions so we will seperate each one out by it's specific version 

There maybe tons of different versions of this code so expect a lot of repeated 
sections, The behavior of all these functions need to be exact. the only
changes made is that each of these require a void* module-state and 
Freethreading is a bit more unsafe.
*/

static int
future_schedule_callbacks(void* state, FutureObj *fut);

#ifdef CPYTHON_ASYNCIO_39
static int
future_schedule_callbacks(void* state, FutureObj *fut)
{
    Py_ssize_t len;
    Py_ssize_t i;

    if (fut->fut_callback0 != NULL) {
        /* There's a 1st callback */

        int ret = call_soon(
            state, fut->fut_loop, fut->fut_callback0,
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

        if (call_soon(state, fut->fut_loop, cb, (PyObject *)fut, ctx)) {
            /* If an error occurs in pure-Python implementation,
               all callbacks are cleared. */
            Py_CLEAR(fut->fut_callbacks);
            return -1;
        }
    }

    Py_CLEAR(fut->fut_callbacks);
    return 0;
}
#endif


#ifdef CPYTHON_ASYNCIO_310
static int
future_schedule_callbacks(void* state, FutureObj *fut)
{
    Py_ssize_t len;
    Py_ssize_t i;

    if (fut->fut_callback0 != NULL) {
        /* There's a 1st callback */

        int ret = call_soon(
            state,
            fut->fut_loop, fut->fut_callback0,
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

        if (call_soon(state, fut->fut_loop, cb, (PyObject *)fut, ctx)) {
            /* If an error occurs in pure-Python implementation,
               all callbacks are cleared. */
            Py_CLEAR(fut->fut_callbacks);
            return -1;
        }
    }

    Py_CLEAR(fut->fut_callbacks);
    return 0;
}
#endif 

#ifdef CPYTHON_ASYNCIO_311
static int
future_schedule_callbacks(void* state, FutureObj *fut)
{
    Py_ssize_t len;
    Py_ssize_t i;

    if (fut->fut_callback0 != NULL) {
        /* There's a 1st callback */

        int ret = call_soon(
            state, fut->fut_loop, fut->fut_callback0,
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

        if (call_soon(state, fut->fut_loop, cb, (PyObject *)fut, ctx)) {
            /* If an error occurs in pure-Python implementation,
               all callbacks are cleared. */
            Py_CLEAR(fut->fut_callbacks);
            return -1;
        }
    }
    Py_CLEAR(fut->fut_callbacks);
    return 0;
}
#endif

#ifdef CPYTHON_ASYNCIO_312
static int
future_schedule_callbacks(void *state, FutureObj *fut)
{
    if (fut->fut_callback0 != NULL) {
        /* There's a 1st callback */

        // Beware: An evil call_soon could alter fut_callback0 or fut_context0.
        // Since we are anyway clearing them after the call, whether call_soon
        // succeeds or not, the idea is to transfer ownership so that external
        // code is not able to alter them during the call.
        PyObject *fut_callback0 = fut->fut_callback0;
        fut->fut_callback0 = NULL;
        PyObject *fut_context0 = fut->fut_context0;
        fut->fut_context0 = NULL;

        int ret = call_soon(state, fut->fut_loop, fut_callback0,
                            (PyObject *)fut, fut_context0);
        Py_CLEAR(fut_callback0);
        Py_CLEAR(fut_context0);
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

    // Beware: An evil call_soon could change fut->fut_callbacks.
    // The idea is to transfer the ownership of the callbacks list
    // so that external code is not able to mutate the list during
    // the iteration.
    PyObject *callbacks = fut->fut_callbacks;
    fut->fut_callbacks = NULL;
    Py_ssize_t n = PyList_GET_SIZE(callbacks);
    for (Py_ssize_t i = 0; i < n; i++) {
        assert(PyList_GET_SIZE(callbacks) == n);
        PyObject *cb_tup = PyList_GET_ITEM(callbacks, i);
        PyObject *cb = PyTuple_GET_ITEM(cb_tup, 0);
        PyObject *ctx = PyTuple_GET_ITEM(cb_tup, 1);

        if (call_soon(state, fut->fut_loop, cb, (PyObject *)fut, ctx)) {
            Py_DECREF(callbacks);
            return -1;
        }
    }
    Py_DECREF(callbacks);
    return 0;
}
#endif 

#ifdef CPYTHON_ASYNCIO_313

static int
future_schedule_callbacks(void *state, FutureObj *fut)
{
    if (fut->fut_callback0 != NULL) {
        /* There's a 1st callback */

        // Beware: An evil call_soon could alter fut_callback0 or fut_context0.
        // Since we are anyway clearing them after the call, whether call_soon
        // succeeds or not, the idea is to transfer ownership so that external
        // code is not able to alter them during the call.
        PyObject *fut_callback0 = fut->fut_callback0;
        fut->fut_callback0 = NULL;
        PyObject *fut_context0 = fut->fut_context0;
        fut->fut_context0 = NULL;

        int ret = call_soon(state, fut->fut_loop, fut_callback0,
                            (PyObject *)fut, fut_context0);
        Py_CLEAR(fut_callback0);
        Py_CLEAR(fut_context0);
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

    // Beware: An evil call_soon could change fut->fut_callbacks.
    // The idea is to transfer the ownership of the callbacks list
    // so that external code is not able to mutate the list during
    // the iteration.
    PyObject *callbacks = fut->fut_callbacks;
    fut->fut_callbacks = NULL;
    Py_ssize_t n = PyList_GET_SIZE(callbacks);
    for (Py_ssize_t i = 0; i < n; i++) {
        assert(PyList_GET_SIZE(callbacks) == n);
        PyObject *cb_tup = PyList_GET_ITEM(callbacks, i);
        PyObject *cb = PyTuple_GET_ITEM(cb_tup, 0);
        PyObject *ctx = PyTuple_GET_ITEM(cb_tup, 1);

        if (call_soon(state, fut->fut_loop, cb, (PyObject *)fut, ctx)) {
            Py_DECREF(callbacks);
            return -1;
        }
    }
    Py_DECREF(callbacks);
    return 0;
}
#endif

#ifdef CPYTHON_ASYNCIO_314

static int
future_schedule_callbacks(void *state, FutureObj *fut)
{
    _Py_CRITICAL_SECTION_ASSERT_OBJECT_LOCKED(fut);

    assert(fut->fut_state != STATE_PENDING);

    // NOTE: Only Future objects are supported at the moment...
    // if (Task_Check(state, fut)) {
    //     // remove task from linked-list of tasks
    //     // as it is finished now
    //     TaskObj *task = (TaskObj *)fut;
    //     unregister_task(task);
    // }

    if (fut->fut_callback0 != NULL) {
        /* There's a 1st callback */

        // Beware: An evil call_soon could alter fut_callback0 or fut_context0.
        // Since we are anyway clearing them after the call, whether call_soon
        // succeeds or not, the idea is to transfer ownership so that external
        // code is not able to alter them during the call.
        PyObject *fut_callback0 = fut->fut_callback0;
        fut->fut_callback0 = NULL;
        PyObject *fut_context0 = fut->fut_context0;
        fut->fut_context0 = NULL;

        int ret = call_soon(state, fut->fut_loop, fut_callback0,
                            (PyObject *)fut, fut_context0);
        Py_CLEAR(fut_callback0);
        Py_CLEAR(fut_context0);
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

    // Beware: An evil call_soon could change fut->fut_callbacks.
    // The idea is to transfer the ownership of the callbacks list
    // so that external code is not able to mutate the list during
    // the iteration.
    PyObject *callbacks = fut->fut_callbacks;
    fut->fut_callbacks = NULL;
    Py_ssize_t n = PyList_GET_SIZE(callbacks);
    for (Py_ssize_t i = 0; i < n; i++) {
        assert(PyList_GET_SIZE(callbacks) == n);
        PyObject *cb_tup = PyList_GET_ITEM(callbacks, i);
        PyObject *cb = PyTuple_GET_ITEM(cb_tup, 0);
        PyObject *ctx = PyTuple_GET_ITEM(cb_tup, 1);

        if (call_soon(state, fut->fut_loop, cb, (PyObject *)fut, ctx)) {
            Py_DECREF(callbacks);
            return -1;
        }
    }
    Py_DECREF(callbacks);
    return 0;
}
#endif

#ifdef CPYTHON_ASYNCIO_315

static int
future_schedule_callbacks(void *state, FutureObj *fut)
{
    _Py_CRITICAL_SECTION_ASSERT_OBJECT_LOCKED(fut);

    assert(fut->fut_state != STATE_PENDING);

    // if (Task_Check(state, fut)) {
    //     // remove task from linked-list of tasks
    //     // as it is finished now
    //     TaskObj *task = (TaskObj *)fut;
    //     unregister_task(task);
    // }

    if (fut->fut_callback0 != NULL) {
        /* There's a 1st callback */

        // Beware: An evil call_soon could alter fut_callback0 or fut_context0.
        // Since we are anyway clearing them after the call, whether call_soon
        // succeeds or not, the idea is to transfer ownership so that external
        // code is not able to alter them during the call.
        PyObject *fut_callback0 = fut->fut_callback0;
        fut->fut_callback0 = NULL;
        PyObject *fut_context0 = fut->fut_context0;
        fut->fut_context0 = NULL;

        int ret = call_soon(state, fut->fut_loop, fut_callback0,
                            (PyObject *)fut, fut_context0);
        Py_CLEAR(fut_callback0);
        Py_CLEAR(fut_context0);
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

    // Beware: An evil call_soon could change fut->fut_callbacks.
    // The idea is to transfer the ownership of the callbacks list
    // so that external code is not able to mutate the list during
    // the iteration.
    PyObject *callbacks = fut->fut_callbacks;
    fut->fut_callbacks = NULL;
    Py_ssize_t n = PyList_GET_SIZE(callbacks);
    for (Py_ssize_t i = 0; i < n; i++) {
        assert(PyList_GET_SIZE(callbacks) == n);
        PyObject *cb_tup = PyList_GET_ITEM(callbacks, i);
        PyObject *cb = PyTuple_GET_ITEM(cb_tup, 0);
        PyObject *ctx = PyTuple_GET_ITEM(cb_tup, 1);

        if (call_soon(state, fut->fut_loop, cb, (PyObject *)fut, ctx)) {
            Py_DECREF(callbacks);
            return -1;
        }
    }
    Py_DECREF(callbacks);
    return 0;
}
#endif


#ifdef CPYTHON_ASYNCIO_316

static int
future_schedule_callbacks(void *state, FutureObj *fut)
{
    _Py_CRITICAL_SECTION_ASSERT_OBJECT_LOCKED(fut);

    assert(fut->fut_state != STATE_PENDING);

    if (Task_Check(state, fut)) {
        // remove task from linked-list of tasks
        // as it is finished now
        TaskObj *task = (TaskObj *)fut;
        unregister_task(task);
    }

    if (fut->fut_callback0 != NULL) {
        /* There's a 1st callback */

        // Beware: An evil call_soon could alter fut_callback0 or fut_context0.
        // Since we are anyway clearing them after the call, whether call_soon
        // succeeds or not, the idea is to transfer ownership so that external
        // code is not able to alter them during the call.
        PyObject *fut_callback0 = fut->fut_callback0;
        fut->fut_callback0 = NULL;
        PyObject *fut_context0 = fut->fut_context0;
        fut->fut_context0 = NULL;

        int ret = call_soon(state, fut->fut_loop, fut_callback0,
                            (PyObject *)fut, fut_context0);
        Py_CLEAR(fut_callback0);
        Py_CLEAR(fut_context0);
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

    // Beware: An evil call_soon could change fut->fut_callbacks.
    // The idea is to transfer the ownership of the callbacks list
    // so that external code is not able to mutate the list during
    // the iteration.
    PyObject *callbacks = fut->fut_callbacks;
    fut->fut_callbacks = NULL;
    Py_ssize_t n = PyList_GET_SIZE(callbacks);
    for (Py_ssize_t i = 0; i < n; i++) {
        assert(PyList_GET_SIZE(callbacks) == n);
        PyObject *cb_tup = PyList_GET_ITEM(callbacks, i);
        PyObject *cb = PyTuple_GET_ITEM(cb_tup, 0);
        PyObject *ctx = PyTuple_GET_ITEM(cb_tup, 1);

        if (call_soon(state, fut->fut_loop, cb, (PyObject *)fut, ctx)) {
            Py_DECREF(callbacks);
            return -1;
        }
    }
    Py_DECREF(callbacks);
    return 0;
}
#endif

/* future_schedule_callbacks */


/* future_set_result hasn't nessesarly changed so it's worth using 
will change a few things so we can take advantage of faster responses */
static int
future_set_result(void *state, FutureObj *fut, PyObject *res)
{
    _Py_CRITICAL_SECTION_ASSERT_OBJECT_LOCKED(fut);

    if (future_ensure_alive(fut)) {
        return -1;
    }

    if (fut->fut_state != STATE_PENDING) {
        PyErr_SetString(ms_cast(state)->asyncio_InvalidStateError, "invalid state");
        return -1;
    }

#ifdef CPYTHON_ASYNCIO_39
    assert(!fut->fut_result);
    Py_INCREF(res);
    fut->fut_result = res;
    fut->fut_state = STATE_FINISHED;
#else
    assert(!fut->fut_result);
    fut->fut_result = Py_NewRef(res);
    fut->fut_state = STATE_FINISHED;
#endif
    if (future_schedule_callbacks(state, fut) == -1) {
        return -1;
    }
    return 0;
}

/* There is still some version differences accross 3.9 vs the others so will change a few things in here. */
static int
future_set_exception(void *state, FutureObj *fut, PyObject *exc);

#ifdef CPYTHON_ASYNCIO_39
static int
future_set_exception(void* state, FutureObj *fut, PyObject *exc)
{
    PyObject *exc_val = NULL;

    if (fut->fut_state != STATE_PENDING) {
        PyErr_SetString(ms_cast(state)->asyncio_InvalidStateError, "invalid state");
        return -1;
    }

    if (PyExceptionClass_Check(exc)) {
        exc_val = PyObject_CallNoArgs(exc);
        if (exc_val == NULL) {
            return -1;
        }
        if (fut->fut_state != STATE_PENDING) {
            Py_DECREF(exc_val);
            PyErr_SetString(ms_cast(state)->asyncio_InvalidStateError, "invalid state");
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
    fut->fut_exception = exc_val;
    fut->fut_state = STATE_FINISHED;

    if (future_schedule_callbacks(fut) == -1) {
        return -1;
    }

    fut->fut_log_tb = 1;
    return 0;
}
#else 

/* Only change was how traceback is handled. */
static int
future_set_exception(void *state, FutureObj *fut, PyObject *exc)
{
    _Py_CRITICAL_SECTION_ASSERT_OBJECT_LOCKED(fut);

    PyObject *exc_val = NULL;

    if (fut->fut_state != STATE_PENDING) {
        PyErr_SetString(ms_cast(state)->asyncio_InvalidStateError, "invalid state");
        return -1;
    }

    if (PyExceptionClass_Check(exc)) {
        exc_val = PyObject_CallNoArgs(exc);
        if (exc_val == NULL) {
            return -1;
        }
        if (fut->fut_state != STATE_PENDING) {
            Py_DECREF(exc_val);
            PyErr_SetString(ms_cast(state)->asyncio_InvalidStateError, "invalid state");
            return -1;
        }
    }
    else {
        exc_val = Py_NewRef(exc);
    }
    if (!PyExceptionInstance_Check(exc_val)) {
        Py_DECREF(exc_val);
        PyErr_SetString(PyExc_TypeError, "invalid exception object");
        return -1;
    }
    if (PyErr_GivenExceptionMatches(exc_val, PyExc_StopIteration)) {
        const char *msg = "StopIteration interacts badly with "
                          "generators and cannot be raised into a "
                          "Future";
        PyObject *message = PyUnicode_FromString(msg);
        if (message == NULL) {
            Py_DECREF(exc_val);
            return -1;
        }
        PyObject *err = PyObject_CallOneArg(PyExc_RuntimeError, message);
        Py_DECREF(message);
        if (err == NULL) {
            Py_DECREF(exc_val);
            return -1;
        }
        assert(PyExceptionInstance_Check(err));

        PyException_SetCause(err, Py_NewRef(exc_val));
        PyException_SetContext(err, Py_NewRef(exc_val));
        Py_DECREF(exc_val);
        exc_val = err;
    }

    assert(!fut->fut_exception);
    assert(!fut->fut_exception_tb);
    fut->fut_exception = exc_val;
    fut->fut_exception_tb = PyException_GetTraceback(exc_val);
    fut->fut_state = STATE_FINISHED;

    if (future_schedule_callbacks(state, fut) == -1) {
        return -1;
    }

    fut->fut_log_tb = 1;
    return 0;
}
#endif


#ifdef CPYTHON_ASYNCIO_39
static int
future_cancel(void* state, FutureObj *fut, PyObject *msg)
{
    fut->fut_log_tb = 0;

    if (fut->fut_state != STATE_PENDING) {
        return 0;
    }
    fut->fut_state = STATE_CANCELLED;

    Py_XINCREF(msg);
    Py_XSETREF(fut->fut_cancel_msg, msg);

    if (future_schedule_callbacks(state, fut) == -1) {
        return -1;
    }
    return 1;
}
#else 
static int
future_cancel(void *state, FutureObj *fut, PyObject *msg)
{
    _Py_CRITICAL_SECTION_ASSERT_OBJECT_LOCKED(fut);

    fut->fut_log_tb = 0;

    if (fut->fut_state != STATE_PENDING) {
        return 0;
    }
    fut->fut_state = STATE_CANCELLED;

    Py_XINCREF(msg);
    Py_XSETREF(fut->fut_cancel_msg, msg);

    if (future_schedule_callbacks(state, fut) == -1) {
        return -1;
    }

    return 1;
}
#endif



/// @brief sets a result to a Future type object, if objects do not
/// match the _asyncio library's object then set_result method 
/// is called as fallback.
/// @param state ptr from capsule
/// @param fut asyncio.Future object
/// @param res result to set.
/// @return -1 on failure, 0 on success.
static int
Future_SetResult(void* state, PyObject* fut, PyObject* res){
    /* XXX: Don't try and risk tasks leaking into the C-API! Use
    fallback to simulate it's errors. */
    if (!Future_Check(ms_cast(state), fut)){
        PyObject* temp;
        temp = PyObject_CallMethodOneArg(fut, ms_cast(state)->intern__set_result, res);
        if (temp == NULL){
            return -1;
        }
        Py_DECREF(temp);
        return 0;
    }
    /* Use the faster call */
    return future_set_result(state, (FutureObj*)fut, res);
}

/// @brief sets an exception to a Future object
/// implements fallback for custom future types.
/// @param state ptr from capsule
/// @param fut future object to set an exception for
/// @param exc exception to be set.
/// @return -1 on failure, 0 on success.
static int 
Future_SetException(void* state, PyObject* fut, PyObject* exc){
    if (!Future_Check(ms_cast(state), fut)){
        PyObject* temp;
        temp = PyObject_CallMethodOneArg(fut, ms_cast(state)->intern__cancel, exc);
        if (temp == NULL){
            return -1;
        }
        Py_DECREF(temp);
        return 0;
    }
    return future_set_exception(state, (FutureObj*)fut, exc);
}

/// @brief cancels an awaitable object.
/// NOTE that only _asyncio.Future types are supported 
/// otherwise fallback gets initiated.
/// @param state ptr from capsule
/// @param fut future object to cancel.
/// @param msg message to set for cancellation, This can be NULL.
/// @return -1 on failure, 1 if cancelled, 0 if already.
static int
Future_Cancel(void* state, PyObject* fut, PyObject* msg) {
    if (!Future_Check(ms_cast(state), fut)){
        int ret = -1;
        PyObject* temp;
        temp = PyObject_CallMethodOneArg(fut, ms_cast(state)->intern__cancel, msg);
        if (temp == NULL){
            return ret;
        }
        ret = PyObject_IsTrue(temp);
        Py_DECREF(temp);
        return ret;
    }
    return future_cancel(state, (FutureObj*)fut, msg);
}



#ifdef __cplusplus
}
#endif



#endif // __AIOAPI_H__