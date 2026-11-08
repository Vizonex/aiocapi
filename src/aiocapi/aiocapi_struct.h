#ifndef __AIOCAPI_STRUCT_H__
#define __AIOCAPI_STRUCT_H__

#include "Python.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AIOCAPI_MODULE_NAME "aiocapi._aiocapi"
#define AIOCAPI_CAPI_NAME "CAPI"
#define AIOCAPI_CAPSULE_NAME AIOCAPI_MODULE_NAME "." AIOCAPI_CAPI_NAME

/* Like Multidict a Version number is used to track the current version being used
as a safety measure, when a new struct field is added in this must be changed!!! */
#define AIOCAPI_CAPI_VERSION 1

/* Used for determining Future's current status as well as 
    what phase is current in process */

typedef enum {
    /* FUTURE_ERROR (not in CPython) is custom and it's a way to 
    detect failure. */
    FUTURE_ERROR = -1,
    FUTURE_STATE_PENDING = 0,
    FUTURE_STATE_CANCELLED = 1,
    FUTURE_STATE_FINISHED = 2
} FutureState;

/* call_soon is left unimplemented since it uses unstable apis and
even implementing them could be slow. So it's left up to the author this makes
event-loops like winloop take advantage of these improvements */
typedef int (*aiocapi_call_soon_cb)(void* user_data, PyObject *loop, PyObject *func, PyObject *arg, PyObject *ctx);

/* TODO: Python version flags. This will be intentionally done when
a new version of python comes out someone will immediately be able to figure
that out, throw an issue and so on and so forth... */

typedef struct {
    int api_version;
    void* state;

    PyTypeObject* FutureType;
    PyTypeObject* TaskType;


    /* _asyncio.Future apis */
    
    // PyObject* (*Future_New)(void* state, PyObject* loop);
    PyObject* (*Future_GetLoop)(void* state, PyObject* self);

    FutureState (*Future_GetState)(void* state, PyObject* self);
    int (*Future_Done)(void* state, PyObject* self);
    /* Cbs are an improvemnet caller/shortcut for those who need  
        to utilize a faster loop.call_soon function. */
    int (*Future_CancelCb)(void* state, PyObject* self, PyObject* message, void* user_data, aiocapi_call_soon_cb call_soon_func);
    int (*Future_Cancel)(void* state, PyObject* self, PyObject* message);
    int (*Future_Cancelled)(void* state, PyObject* self);
    PyObject* (*Future_Result)(void* state, PyObject* self);
    PyObject* (*Future_Exception)(void* state, PyObject* self);
    /* Primarly, it is used as a shortcut for Cython 
    because NULL checking a PyObject* isn't a reliable solution
    unless you are intrested in unwanted surprises :) */
    int (*Future_HasException)(void* state, PyObject* self);
    int (*Future_SetResultCb)(void* state, PyObject* self, PyObject* result, void* user_data, aiocapi_call_soon_cb call_soon_func);
    int (*Future_SetResult)(void* state, PyObject* self, PyObject* result);
    int (*Future_SetExceptionCb)(void* state, PyObject* self, PyObject* exception, void* user_data, aiocapi_call_soon_cb call_soon_func);
    int (*Future_SetException)(void* state, PyObject* self, PyObject* exception);

} AioCAPI;

#ifdef __cplusplus
}
#endif

#endif /* __AIOCAPI_STRUCT_H__ */