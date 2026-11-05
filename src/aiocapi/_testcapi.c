#include <Python.h>
#include "aiocapi.h"

/* This is meant to be an example of what is possible. 
 * Cython is the recommened approch if you want to save yourself
 from any bigger amounts of pain. */

typedef struct {
    AioCAPI* capi;
} mod_state;

static inline mod_state*
get_mod_state(PyObject* mod)
{
    mod_state* state = (mod_state*)PyModule_GetState(mod);
    assert(state != NULL);
    return state;
}

static PyObject*
future_get_loop(PyObject* self, PyObject* arg){
    mod_state* state = get_mod_state(self);
    return (PyObject*)Future_GetLoop(state->capi, arg);
}

static PyObject*
future_get_state(PyObject* self, PyObject* arg){
    mod_state* state = get_mod_state(self);
    return PyLong_FromLong((long)Future_GetState(state->capi, arg));
}

static PyObject*
future_set_result(PyObject* self, PyObject* const* args, Py_ssize_t nargs)
{
    if (nargs != 2) {
        PyErr_SetString(PyExc_TypeError,
                        "future_set_result should be called with self and result");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    
    int ret = Future_SetResult(state->capi, args[0], args[1]);
    if (ret < 0){
        return NULL;
    }
    Py_RETURN_NONE;
}

static int
module_traverse(PyObject* mod, visitproc visit, void* arg)
{
    return 0;
}

static int
module_clear(PyObject* mod)
{
    return 0;
}

static void
module_free(void* mod)
{
    (void)module_clear((PyObject*)mod);
}

static PyMethodDef module_methods[] = {
    {"future_get_loop", (PyCFunction)future_get_loop, METH_O},
    {"future_get_state", (PyCFunction)future_get_state, METH_O},
    {"future_set_result", (PyCFunction)future_set_result, METH_FASTCALL},
    {NULL, NULL} /* sentinel */
};

static int module_exec(PyObject* mod){
    mod_state* state = get_mod_state(mod);
    state->capi = AioCAPI_GetCAPI();
    if (state->capi == NULL) {
        return -1;
    }
    return 0;
}


static struct PyModuleDef_Slot module_slots[] = {
    {Py_mod_exec, module_exec},
#if PY_VERSION_HEX >= 0x030c00f0
    {Py_mod_multiple_interpreters, Py_MOD_PER_INTERPRETER_GIL_SUPPORTED},
#endif
#if PY_VERSION_HEX >= 0x030d00f0
    {Py_mod_gil, Py_MOD_GIL_NOT_USED},
#endif
    {0, NULL},
};

static PyModuleDef testcapi_module = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = "_testcapi",
    .m_size = sizeof(mod_state),
    .m_methods = module_methods,
    .m_slots = module_slots,
    .m_traverse = module_traverse,
    .m_clear = module_clear,
    .m_free = (freefunc)module_free,
};

PyMODINIT_FUNC
PyInit__testcapi(void)
{
    return PyModuleDef_Init(&testcapi_module);
}

