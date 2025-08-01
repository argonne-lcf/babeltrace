/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2019 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2019 Philippe Proulx <pproulx@efficios.com>
 * Copyright (c) 2019 Simon Marchi <simon.marchi@efficios.com>
 */

#define BT_LOG_OUTPUT_LEVEL log_level
#define BT_LOG_TAG          "PY-COMMON"
#include <Python.h>
#include <stdbool.h>

#include "logging/log.h"

#include "common/assert.h"

#include "py-common.h"
#include "py-common.hpp"

/*
 * Concatenate strings in list `py_str_list`, returning the result as a
 * GString.  Remove the trailing \n, if the last string ends with a \n.
 */
static GString *py_str_list_to_gstring(PyObject *py_str_list, int log_level)
{
    Py_ssize_t i;
    GString *gstr;

    gstr = g_string_new(NULL);
    if (!gstr) {
        BT_LOGE("Failed to allocate a GString.");
        goto end;
    }

    for (i = 0; i < PyList_Size(py_str_list); i++) {
        PyObject *py_str;
        const char *str;

        py_str = PyList_GetItem(py_str_list, i);
        BT_ASSERT(py_str);
        BT_ASSERT(PyUnicode_CheckExact(py_str));

        /* `str` is owned by `py_str`, not by us */
        str = PyUnicode_AsUTF8(py_str);
        if (!str) {
            if (BT_LOG_ON_ERROR) {
                BT_LOGE_STR("PyUnicode_AsUTF8() failed:");
                PyErr_Print();
            }

            goto error;
        }

        /* `str` has a trailing newline */
        g_string_append(gstr, str);
    }

    if (gstr->len > 0) {
        /* Remove trailing newline if any */
        if (gstr->str[gstr->len - 1] == '\n') {
            g_string_truncate(gstr, gstr->len - 1);
        }
    }

    goto end;

error:
    g_string_free(gstr, TRUE);
    gstr = NULL;

end:
    return gstr;
}

GString *bt_py_common_format_tb(PyObject *py_exc_tb, int log_level)
{
    PyObject *traceback_module = NULL;
    PyObject *format_tb_func = NULL;
    PyObject *exc_str_list = NULL;
    GString *msg_buf = NULL;

    BT_ASSERT(py_exc_tb);

    /*
     * Import the standard `traceback` module which contains the
     * functions to format a traceback.
     */
    traceback_module = PyImport_ImportModule("traceback");
    if (!traceback_module) {
        BT_LOGE_STR("Failed to import `traceback` module.");
        goto error;
    }

    format_tb_func = PyObject_GetAttrString(traceback_module, "format_tb");
    if (!format_tb_func) {
        BT_LOGE("Cannot find `format_tb` attribute in `traceback` module.");
        goto error;
    }

    if (!PyCallable_Check(format_tb_func)) {
        BT_LOGE("`traceback.format_tb` attribute is not callable.");
        goto error;
    }

    /*
     * If we are calling format_exception_only, `py_exc_tb` is NULL, so the
     * effective argument list is of length 2.
     */
    exc_str_list = PyObject_CallFunctionObjArgs(format_tb_func, py_exc_tb, NULL);
    if (!exc_str_list) {
        if (BT_LOG_ON_ERROR) {
            BT_LOGE("Failed to call `traceback.format_tb` function:");
            PyErr_Print();
        }

        goto error;
    }

    msg_buf = py_str_list_to_gstring(exc_str_list, log_level);
    if (!msg_buf) {
        goto error;
    }

error:
    Py_XDECREF(traceback_module);
    Py_XDECREF(format_tb_func);
    Py_XDECREF(exc_str_list);

    return msg_buf;
}

GString *bt_py_common_format_exception(PyObject *py_exc_type, PyObject *py_exc_value,
                                       PyObject *py_exc_tb, int log_level, bool chain)
{
    PyObject *traceback_module = NULL;
    PyObject *format_exception_func = NULL;
    PyObject *exc_str_list = NULL;
    GString *msg_buf = NULL;
    const char *format_exc_func_name;

    /*
     * Import the standard `traceback` module which contains the
     * functions to format an exception.
     */
    traceback_module = PyImport_ImportModule("traceback");
    if (!traceback_module) {
        BT_LOGE_STR("Failed to import `traceback` module.");
        goto error;
    }

    /*
     * `py_exc_tb` can be `NULL`, when we fail to call a Python
     * function from native code (there is no Python stack at that
     * point). For example, a function which takes 5 positional
     * arguments, but 8 were given.
     */
    format_exc_func_name = py_exc_tb ? "format_exception" : "format_exception_only";
    format_exception_func = PyObject_GetAttrString(traceback_module, format_exc_func_name);
    if (!format_exception_func) {
        BT_LOGE("Cannot find `%s` attribute in `traceback` module.", format_exc_func_name);
        goto error;
    }

    if (!PyCallable_Check(format_exception_func)) {
        BT_LOGE("`traceback.%s` attribute is not callable.", format_exc_func_name);
        goto error;
    }

    /*
     * If we are calling format_exception_only, `py_exc_tb` is NULL, so the
     * effective argument list is of length 2.
     */
    exc_str_list = PyObject_CallFunctionObjArgs(format_exception_func, py_exc_type, py_exc_value,
                                                py_exc_tb, Py_None /* limit */,
                                                chain ? Py_True : Py_False /* chain */, NULL);
    if (!exc_str_list) {
        if (BT_LOG_ON_ERROR) {
            BT_LOGE("Failed to call `traceback.%s` function:", format_exc_func_name);
            PyErr_Print();
        }

        goto error;
    }

    msg_buf = py_str_list_to_gstring(exc_str_list, log_level);
    if (!msg_buf) {
        goto error;
    }

error:
    Py_XDECREF(exc_str_list);
    Py_XDECREF(format_exception_func);
    Py_XDECREF(traceback_module);

    return msg_buf;
}

GString *bt_py_common_format_current_exception(int log_level)
{
    GString *result;
    PyObject *py_exc_type = NULL;
    PyObject *py_exc_value = NULL;
    PyObject *py_exc_tb = NULL;

    BT_ASSERT(PyErr_Occurred());
    PyErr_Fetch(&py_exc_type, &py_exc_value, &py_exc_tb);
    BT_ASSERT(py_exc_type);

    /*
     * Make sure `py_exc_value` is what we expected: an instance of
     * `py_exc_type`.
     */
    PyErr_NormalizeException(&py_exc_type, &py_exc_value, &py_exc_tb);

    result = bt_py_common_format_exception(py_exc_type, py_exc_value, py_exc_tb, log_level, true);

    /*
     * We can safely call PyErr_Restore() because we always call
     * PyErr_Fetch(), and having an error indicator is a function's
     * precondition.
     *
     * PyErr_Restore() steals our references.
     */
    PyErr_Restore(py_exc_type, py_exc_value, py_exc_tb);

    return result;
}

std::vector<const char *> btPyStrListToVector(PyObject *py_str_list)
{
    BT_ASSERT(PyList_Check(py_str_list));

    std::vector<const char *> strings;

    for (Py_ssize_t i = 0; i < PyList_Size(py_str_list); ++i) {
        PyObject *py_str = PyList_GetItem(py_str_list, i);

        BT_ASSERT(PyUnicode_Check(py_str));
        strings.emplace_back(PyUnicode_AsUTF8(py_str));
    }

    return strings;
}
