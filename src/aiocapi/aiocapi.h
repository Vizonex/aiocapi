#ifndef __AIOCAPI_H__
#define __AIOCAPI_H__


#ifdef __cplusplus
extern "C" {
#endif

#include "Python.h"
#include "aiocapi_struct.h"

/* Same as multidict this was intentially done to save the maintainers
some delicate time. */

static inline int
_AioCAPI_CheckAPIVersion(AioCAPI* capi){
    if (capi->api_version < AIOCAPI_CAPI_VERSION) {
        PyErr_Format(PyExc_RuntimeError,
                     "aiocapi C API version mismatch: this code was built "
                     "against aiocapi_capi.h version %d, but the "
                     "installed aiocapi only provides version %d; "
                     "upgrade aiocapi",
                     AIOCAPI_CAPI_VERSION,
                     capi->api_version);
        return -1;
    }
    return 0;
}

static inline AioCAPI*
AioCAPI_GetCAPI(void)
{
    AioCAPI* capi =
        (AioCAPI*)PyCapsule_Import(AIOCAPI_CAPSULE_NAME, 0);
    if (capi == NULL) {
        return NULL;
    }
    if (_AioCAPI_CheckAPIVersion(capi) < 0) {
        return NULL;
    }
    return capi;
}

static int 
Future_Cancel(AioCAPI* capi, PyObject* self, PyObject* message){
    return capi->Future_Cancel(capi->state, self, message);
}

static int 
Future_SetException(AioCAPI* capi, PyObject *self, PyObject *result){
    return capi->Future_SetException(capi->state, self, result);
}

static int 
Future_SetResult(AioCAPI* capi, PyObject* self, PyObject* res){
    return capi->Future_SetResult(capi->state, self, res);
}

static int
Future_CheckExact(AioCAPI* capi, PyObject* obj){
    return Py_IS_TYPE(obj, capi->FutureType);
}

static int
Future_Check(AioCAPI* capi, PyObject* obj){
    return (Future_CheckExact(capi, obj) || PyObject_TypeCheck(obj, capi->FutureType));
}

#ifdef __cplusplus
}
#endif


#endif // __AIOCAPI_H__