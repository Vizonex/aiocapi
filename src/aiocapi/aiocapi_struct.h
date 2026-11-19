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


/* TODO: Python version flags. This will be intentionally done when
a new version of python comes out someone will immediately be able to figure
that out, throw an issue and so on and so forth... */

typedef struct {
    int api_version;
    void* state;

    PyTypeObject* FutureType;
    PyTypeObject* TaskType;

    /* _asyncio.Future apis */
    int (*Future_SetResult)(void* state, PyObject* fut, PyObject* res);
    int (*Future_SetException)(void* state, PyObject* fut, PyObject* exc);
    int (*Future_Cancel)(void* state, PyObject* fut, PyObject* msg);
} AioCAPI;

#ifdef __cplusplus
}
#endif

#endif /* __AIOCAPI_STRUCT_H__ */