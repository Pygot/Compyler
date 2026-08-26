#ifndef CPY_PYAPI_H
#define CPY_PYAPI_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>

typedef void      *PyObj;
typedef ptrdiff_t  cpy_ssize;

#define CPY_FILE_INPUT 257
#define CPY_MARSHAL_VERSION 4

#define CPY_PY_REQUIRED(X)                                                    \
    X(void,       Py_InitializeEx,               (int))                       \
    X(int,        Py_FinalizeEx,                 (void))                      \
    X(int,        Py_IsInitialized,              (void))                      \
    X(PyObj,      PyImport_AddModule,            (const char *))              \
    X(PyObj,      PyImport_ImportModule,         (const char *))              \
    X(PyObj,      PyModule_GetDict,              (PyObj))                     \
    X(int,        PyDict_SetItemString,          (PyObj, const char *, PyObj))\
    X(PyObj,      PyDict_GetItemString,          (PyObj, const char *))       \
    X(PyObj,      PyDict_New,                    (void))                      \
    X(PyObj,      PyEval_EvalCode,               (PyObj, PyObj, PyObj))       \
    X(PyObj,      PyMarshal_ReadObjectFromString,(const char *, cpy_ssize))   \
    X(PyObj,      PyUnicode_FromString,          (const char *))              \
    X(PyObj,      PyUnicode_FromWideChar,        (const wchar_t *, cpy_ssize))\
    X(const char *, PyUnicode_AsUTF8,            (PyObj))                     \
    X(PyObj,      PyList_New,                    (cpy_ssize))                 \
    X(int,        PyList_Append,                 (PyObj, PyObj))              \
    X(cpy_ssize,  PyList_Size,                   (PyObj))                     \
    X(PyObj,      PyList_GetItem,                (PyObj, cpy_ssize))          \
    X(int,        PyList_Insert,                 (PyObj, cpy_ssize, PyObj))   \
    X(cpy_ssize,  PyTuple_Size,                  (PyObj))                     \
    X(PyObj,      PyTuple_GetItem,               (PyObj, cpy_ssize))          \
    X(int,        PySys_SetObject,               (const char *, PyObj))       \
    X(PyObj,      PySys_GetObject,               (const char *))              \
    X(void,       Py_DecRef,                     (PyObj))                     \
    X(void,       Py_IncRef,                     (PyObj))                     \
    X(void,       PyErr_Print,                   (void))                      \
    X(void,       PyErr_Clear,                   (void))                      \
    X(PyObj,      PyErr_Occurred,                (void))                      \
    X(PyObj,      PyObject_GetAttrString,        (PyObj, const char *))       \
    X(int,        PyObject_SetAttrString,        (PyObj, const char *, PyObj))\
    X(PyObj,      PyBool_FromLong,               (long))                      \
    X(char *,     PyBytes_AsString,              (PyObj))                     \
    X(cpy_ssize,  PyBytes_Size,                  (PyObj))                     \
    X(int,        PyObject_IsInstance,           (PyObj, PyObj))              \
    X(PyObj,      PyObject_GetIter,              (PyObj))                     \
    X(PyObj,      PyIter_Next,                   (PyObj))                     \
    X(long,       PyLong_AsLong,                 (PyObj))                     \
    X(long long,  PyLong_AsLongLong,             (PyObj))                     \
    X(PyObj,      PyLong_FromLong,               (long))                      \
    X(int,        PyObject_IsTrue,               (PyObj))                     \
    X(PyObj,      PyTuple_New,                   (cpy_ssize))                 \
    X(int,        PyTuple_SetItem,               (PyObj, cpy_ssize, PyObj))   \
    X(PyObj,      PyObject_Call,                 (PyObj, PyObj, PyObj))       \
    X(double,     PyFloat_AsDouble,              (PyObj))

#define CPY_PY_OPTIONAL(X)                                                    \
    X(PyObj,      Py_CompileStringExFlags,       (const char *, const char *, int, void *, int)) \
    X(PyObj,      PyMarshal_WriteObjectToString, (PyObj, int))                \
    X(long,       PyImport_GetMagicNumber,       (void))                      \
    X(int,        PyRun_SimpleString,            (const char *))

#define CPY_DECL_PTR(ret, name, args) ret (*name) args;

typedef struct {
    wchar_t dll_name[64];
    int     version;
    CPY_PY_REQUIRED(CPY_DECL_PTR)
    CPY_PY_OPTIONAL(CPY_DECL_PTR)
} cpy_py;

int  cpy_py_bind(cpy_py *py, HMODULE dll);
int  cpy_py_load(cpy_py *py, const wchar_t *dll_path);

#endif
