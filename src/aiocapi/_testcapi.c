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

#define RETURN_STATUS_AS_BOOL(ret)  \
    if (ret < 0) return NULL;       \
    return PyBool_FromLong(ret)

#define RETURN_STATUS_AS_NONE(ret)  \
    if (ret < 0) return NULL;       \
    Py_RETURN_NONE



static PyObject*
future_get_loop(PyObject* self, PyObject* arg){
    mod_state* state = get_mod_state(self);
    return (PyObject*)Future_GetLoop(state->capi, arg);
}

static PyObject*
future_get_state(PyObject* self, PyObject* arg){
    mod_state* state = get_mod_state(self);
    FutureState fut_state = Future_GetState(state->capi, arg);
    if (fut_state == FUTURE_ERROR){
        return NULL;
    }
    return PyLong_FromLong((long)fut_state);
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
    RETURN_STATUS_AS_NONE(ret);
}

static PyObject* 
future_done(PyObject* self, PyObject* arg){
    mod_state* state = get_mod_state(self);
    int ret = Future_Done(state->capi, arg);
    RETURN_STATUS_AS_BOOL(ret);
}

static PyObject*
future_cancel(PyObject* self, PyObject* arg){
    mod_state* state = get_mod_state(self);
    int ret = Future_Cancel(state->capi, arg, NULL);
    RETURN_STATUS_AS_BOOL(ret);
}

static PyObject*
future_cancel_with_message(PyObject* self, PyObject* const* args, Py_ssize_t nargs){
    if (nargs != 2) {
        PyErr_SetString(PyExc_TypeError,
                        "future_cancel_with_message should be called with self and message");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    int ret = Future_Cancel(state->capi, args[0], args[1]);
    RETURN_STATUS_AS_BOOL(ret);
}

static PyObject*
future_cancelled(PyObject* self, PyObject* arg){
    mod_state* state = get_mod_state(self);
    int ret = Future_Cancelled(state->capi, arg);
    RETURN_STATUS_AS_BOOL(ret);
}

static PyObject*
future_set_exception(PyObject* self, PyObject* const* args, Py_ssize_t nargs){
    if (nargs != 2) {
        PyErr_SetString(PyExc_TypeError,
                        "future_set_exception should be called with self and exception");
        return NULL;
    }
    mod_state* state = get_mod_state(self);
    int ret = Future_SetException(state->capi, args[0], args[1]);
    RETURN_STATUS_AS_NONE(ret);
}

static PyObject*
future_result(PyObject* self, PyObject* arg){
    mod_state* state = get_mod_state(self);
    return Future_Result(state->capi, arg);
}

static PyObject*
future_exception(PyObject* self, PyObject* arg){
    mod_state* state = get_mod_state(self);
    return Future_Exception(state->capi, arg);
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

/* Function Map for _testcapi as a shortcut 
 * inspired ideas from llhttp and libuv */
#define TESTCAPI_FUNCTION_MAP(XX) \
    XX(future_get_loop, METH_O) \
    XX(future_get_state, METH_O) \
    XX(future_set_result, METH_FASTCALL) \
    XX(future_done, METH_O) \
    XX(future_cancel, METH_O) \
    XX(future_cancel_with_message, METH_FASTCALL) \
    XX(future_cancelled, METH_O) \
    XX(future_set_exception, METH_FASTCALL) \
    XX(future_result, METH_O) \
    XX(future_exception, METH_O)


/* Expands defined module methods */
#define TESTCAPI_FUNCTION_DEF(FUNC, MT) \
    {#FUNC, ((PyCFunction)FUNC), MT},

static PyMethodDef module_methods[] = {
    TESTCAPI_FUNCTION_MAP(TESTCAPI_FUNCTION_DEF)
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

