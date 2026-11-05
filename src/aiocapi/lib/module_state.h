#ifndef __MODULE_STATE_H__
#define __MODULE_STATE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "Python.h"

/*********************************************************************
 * Module State                                                      *
 *********************************************************************/

typedef struct {
    int am_alive; /* Sanity check */
    PyTypeObject* FutureType;
    PyTypeObject* TaskType;
    
    /* important attributes */
    PyObject* asyncio_InvalidStateError;
    PyObject* asyncio_CancelledError;
    PyObject* context_kwname;
} mod_state;

static struct PyModuleDef aiocapi_module;

static mod_state*
get_mod_state(PyObject* mod)
{
    mod_state* state = (mod_state*)PyModule_GetState(mod);
    assert(state != NULL);
    return state;
}

static mod_state*
get_mod_state_globally(){
    PyObject* module = PyState_FindModule(&aiocapi_module);
    return get_mod_state(module);
}


#define Future_CheckExact(state, obj) Py_IS_TYPE(obj, state->FutureType)
#define Future_Check(state, obj) \
    (Future_CheckExact(state, obj) || PyObject_TypeCheck(obj, state->FutureType))
#define Task_CheckExact(state, obj) Py_IS_TYPE(obj, state->TaskType)
#define Task_Check(state, obj) \
    (Task_CheckExact(state, obj) || Py_IS_TYPE(obj, state->TaskType))

/* Based off multidict but it's put here as we have multiple versions of python 
to try and satisfy. */

#define __FUTURE_VALIDATION_CHECK(SELF, STATE, ON_FAIL)                              \
    if (!Future_Check(((mod_state*)STATE), (SELF))) {                                \
        PyErr_Format(PyExc_TypeError,                                                \
                     #SELF " should be a low level _asyncio.Future instance not %s", \
                     Py_TYPE(SELF)->tp_name);                                        \
        return ON_FAIL;                                                              \
    }

#define __TASK_VALIDATION_CHECK(SELF, STATE, ON_FAIL)                              \
    if (!Task_Check(((mod_state*)STATE), (SELF))) {                                \
        PyErr_Format(PyExc_TypeError,                                                \
                     #SELF " should be a low level _asyncio.Task instance not %s", \
                     Py_TYPE(SELF)->tp_name);                                        \
        return ON_FAIL;                                                              \
    }

#ifdef __cplusplus
}
#endif


#endif // __MODULE_STATE_H__