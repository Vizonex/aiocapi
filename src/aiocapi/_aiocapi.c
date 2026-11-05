#define PY_SSIZE_T_CLEAN
#include <Python.h>

/* Library modules (Used for determining what version of python to hack) */

#include "lib/module_state.h"
#include "lib/aio311.h"
#include "lib/pythoncapi_compat.h"
#include "aiocapi_struct.h"

/* Compiler Error should be enough to stop unimplemented python versions */
#ifndef AIO311
    #error "Your Version of Python has not been implemented yet" \
        "please throw an issue with the version of python you are looking for."
#endif


/* Will just put the capsule insatallation here as it's the key
importance of this library, without it that library is nothing. */


static void
_capsule_free(AioCAPI* capi)
{
    PyMem_Free(capi);
}

static void
_capsule_destructor(PyObject* o)
{
    AioCAPI* capi =
        (AioCAPI*)PyCapsule_GetPointer(o, AIOCAPI_CAPSULE_NAME);
    _capsule_free(capi);
    
}

static PyObject*
new_capsule(mod_state* state)
{
    AioCAPI* capi = (AioCAPI*)PyMem_Malloc(sizeof(AioCAPI ));
    // TODO: Compiler Macros like UNLIKELY and LIKELY
    if (capi == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    capi->api_version = AIOCAPI_CAPI_VERSION;
    capi->state = (void*)state;

    capi->FutureType = state->FutureType;
    capi->TaskType = state->TaskType;
    
    // capi->Future_New = Future_New;
    capi->Future_GetLoop = Future_GetLoop;
    capi->Future_GetState = Future_GetState;
    capi->Future_Done = Future_Done;

    capi->Future_Cancel = Future_Cancel;
    capi->Future_CancelCb = Future_CancelCb;
    capi->Future_Cancelled = Future_Cancelled;
    capi->Future_Result = Future_Result;
    capi->Future_Exception = Future_Exception;

    capi->Future_HasException = Future_HasException;

    capi->Future_SetResult = Future_SetResult;
    capi->Future_SetResultCb = Future_SetResultCb;

    capi->Future_SetExceptionCb = Future_SetExceptionCb;
    capi->Future_SetException = Future_SetException;

    PyObject* ret =
        PyCapsule_New(capi, AIOCAPI_CAPSULE_NAME, _capsule_destructor);
    if (ret == NULL) {
        _capsule_free(capi);
    }
    return ret;
}



PyDoc_STRVAR(is_alive__doc__,
"_is_alive()\n"
"--\n"
"Checks if c module is successful and is working properly,\n"
"this will always return True."
);

 /* Function to determine if aiocapi imported properly nothing more or less. */
static PyObject* is_alive(PyObject* self, PyObject* Py_UNUSED(ignored)){
    return PyBool_FromLong(get_mod_state(self)->am_alive);
}

static struct PyMethodDef aiocapi_module_methods[] = {
    {
        "_is_alive", (PyCFunction)is_alive, METH_NOARGS, is_alive__doc__
    },
    {NULL, NULL}
};




static int 
aiocapi_module_clear(PyObject* self){
    mod_state* state = get_mod_state(self);
    Py_CLEAR(state->FutureType);
    Py_CLEAR(state->TaskType);
    Py_CLEAR(state->asyncio_CancelledError);
    Py_CLEAR(state->asyncio_InvalidStateError);
    return 0;
}

static void
aiocapi_module_free(PyObject* self){
    aiocapi_module_clear(self);
}

static int 
aiocapi_module_traverse(PyObject* self, visitproc visit, void* arg){
    mod_state* state = get_mod_state(self);
    Py_VISIT(state->FutureType);
    Py_VISIT(state->TaskType);
    Py_VISIT(state->asyncio_CancelledError);
    Py_VISIT(state->asyncio_InvalidStateError);
    return 0;
}

static struct PyModuleDef aiocapi_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "aiocapi._aiocapi",
    .m_size = sizeof(mod_state),
    .m_methods = aiocapi_module_methods,
    .m_traverse = aiocapi_module_traverse,
    .m_clear = aiocapi_module_clear,
    .m_free = (freefunc)aiocapi_module_free,
};



PyMODINIT_FUNC 
PyInit__aiocapi(void){
    PyObject* module;
    PyObject* temp;
    /* temporary imports go here */
    PyObject* temp_module = NULL;

    mod_state* module_state = NULL;


    module = PyState_FindModule(&aiocapi_module);
    if (module != NULL){
        // puts("PyState_FindModule success!");
        Py_INCREF(module);
        return module;
    }
    module = PyModule_Create(&aiocapi_module);
    if (module == NULL){
        // puts("PyModule_Create failed!");
        goto error;
    }

    module_state = (mod_state*)PyModule_GetState(module);
    if (module_state == NULL){
        // puts("PyModule_GetState failed!");
        goto error;
    }

    /* we have passed out sanity check! */
    module_state->am_alive = 1;

    module_state->context_kwname = Py_BuildValue("(s)", "context");
    if (module_state->context_kwname == NULL) {
        goto error;
    }
    
    
/* Macros originate from the module being synced up (_asynciomodule.c) */
#define WITH_MOD(NAME) \
    Py_CLEAR(temp_module); \
    temp_module = PyImport_ImportModule(NAME); \
    if (temp_module == NULL) { \
        goto error; \
    }

#define GET_MOD_ATTR(VAR, NAME) \
    VAR = PyObject_GetAttrString(temp_module, NAME); \
    if (VAR == NULL) { \
        goto error; \
    }

    
#define SET_OBJECT(VAR, NAME) \
    Py_INCREF(VAR); \
    if (PyModule_AddObject(module, NAME, VAR) < 0){ \
        Py_DECREF(VAR); \
        goto error; \
    }

#define SET_TYPE(VAR, NAME) \
    GET_MOD_ATTR(temp, NAME) \
    SET_OBJECT(temp, NAME) \
    module_state->VAR = (PyTypeObject*)temp;

#define SET_MOD_ATTR(VAR, NAME) \
    GET_MOD_ATTR(temp, NAME) \
    SET_OBJECT(temp, NAME) \
    module_state->VAR = temp;


    /* we want the lower level types not the Pure Python Ones. 
     * Otherwise this improvement and entire reason for this library 
     * becomes useless. */
    WITH_MOD("_asyncio")
    SET_TYPE(FutureType, "Future");
    SET_TYPE(TaskType, "Task");

    WITH_MOD("asyncio.exceptions")
    SET_MOD_ATTR(asyncio_InvalidStateError, "InvalidStateError")
    SET_MOD_ATTR(asyncio_CancelledError, "CancelledError")


    /* Last but not least the final cherry on top. */
    PyObject* capsule = new_capsule(module_state);
    if (capsule == NULL) {
        goto error;
    }
    if (PyModule_Add(module, AIOCAPI_CAPI_NAME, capsule) < 0) {
        goto error;
    }
    return module;
error:
    Py_XDECREF(module);
    Py_XDECREF(temp_module);
    return NULL;

#undef WITH_MOD
#undef GET_MOD_ATTR
#undef SET_OBJECT
#undef SET_TYPE
};
