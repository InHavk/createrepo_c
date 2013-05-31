/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2013  Tomas Mlcoch
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#include <Python.h>
#include <assert.h>
#include <stddef.h>

#include "src/createrepo_c.h"

#include "xml_parser-py.h"
#include "typeconversion.h"
#include "package-py.h"
#include "exception-py.h"

typedef struct {
    PyObject *py_newpkgcb;
    PyObject *py_pkgcb;
    PyObject *py_warningcb;
    PyObject *py_pkg;       /*!< Current processed package */
} CbData;

static int
c_newpkgcb(cr_Package **pkg,
           const char *pkgId,
           const char *name,
           const char *arch,
           void *cbdata,
           GError **err)
{
    PyObject *arglist, *result;
    CbData *data = cbdata;

    if (data->py_pkg) {
        // Decref ref count on previous processed package
        Py_DECREF(data->py_pkg);
        data->py_pkg = NULL;
    }

    arglist = Py_BuildValue("(sss)", pkgId, name, arch);
    result = PyObject_CallObject(data->py_newpkgcb, arglist);
    Py_DECREF(arglist);

    if (result == NULL) {
        // Exception raised
        PyErr_ToGError(err);
        return CR_CB_RET_ERR;
    }

    if (!PackageObject_Check(result) && result != Py_None) {
        PyErr_SetString(PyExc_TypeError,
            "Expected a cr_Package or None as a callback return value");
        Py_DECREF(result);
        return CR_CB_RET_ERR;
    }

    *pkg = Package_FromPyObject(result);
    data->py_pkg = result; // Store reference to current package

    return CR_CB_RET_OK;
}

static int
c_pkgcb(cr_Package *pkg,
        void *cbdata,
        GError **err)
{
    PyObject *result;
    CbData *data = cbdata;

    CR_UNUSED(pkg);

    if (data->py_pkg) {
        // Decref ref count on processed package
        Py_DECREF(data->py_pkg);
        data->py_pkg = NULL;
    }

    result = PyObject_CallObject(data->py_pkgcb, NULL);

    if (result == NULL) {
        // Exception raised
        PyErr_ToGError(err);
        return CR_CB_RET_ERR;
    }

    Py_DECREF(result);
    return CR_CB_RET_OK;
}

static int
c_warningcb(cr_XmlParserWarningType type,
            char *msg,
            void *cbdata,
            GError **err)
{
    PyObject *arglist, *result;
    CbData *data = cbdata;

    arglist = Py_BuildValue("(is)", type, msg);
    result = PyObject_CallObject(data->py_warningcb, arglist);
    Py_DECREF(arglist);

    if (result == NULL) {
        // Exception raised
        PyErr_ToGError(err);
        return CR_CB_RET_ERR;
    }

    Py_DECREF(result);
    return CR_CB_RET_OK;
}

PyObject *
py_xml_parse_primary(PyObject *self, PyObject *args)
{
    CR_UNUSED(self);

    char *filename;
    int do_files;
    PyObject *py_newpkgcb, *py_pkgcb, *py_warningcb;
    CbData cbdata;
    GError *tmp_err = NULL;

    if (!PyArg_ParseTuple(args, "sOOOi:py_xml_parse_primary",
                                         &filename,
                                         &py_newpkgcb,
                                         &py_pkgcb,
                                         &py_warningcb,
                                         &do_files)) {
        return NULL;
    }

    if (!PyCallable_Check(py_newpkgcb)) {
        PyErr_SetString(PyExc_TypeError, "newpkgcb must be callable");
        return NULL;
    }

    if (!PyCallable_Check(py_pkgcb) && py_pkgcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "pkgcb must be callable or None");
        return NULL;
    }

    if (!PyCallable_Check(py_warningcb) && py_warningcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "warningcb must be callable or None");
        return NULL;
    }

    Py_XINCREF(py_newpkgcb);
    Py_XINCREF(py_pkgcb);
    Py_XINCREF(py_warningcb);

    cr_XmlParserPkgCb       ptr_c_pkgcb     = NULL;
    cr_XmlParserWarningCb   ptr_c_warningcb = NULL;

    if (py_pkgcb != Py_None)
        ptr_c_pkgcb = c_pkgcb;
    if (py_warningcb != Py_None)
        ptr_c_warningcb = c_warningcb;

    cbdata.py_newpkgcb  = py_newpkgcb;
    cbdata.py_pkgcb     = py_pkgcb;
    cbdata.py_warningcb = py_warningcb;
    cbdata.py_pkg       = NULL;

    cr_xml_parse_primary(filename,
                         c_newpkgcb,
                         &cbdata,
                         ptr_c_pkgcb,
                         &cbdata,
                         ptr_c_warningcb,
                         &cbdata,
                         do_files,
                         &tmp_err);

    Py_XDECREF(py_newpkgcb);
    Py_XDECREF(py_pkgcb);
    Py_XDECREF(py_warningcb);

    if (tmp_err) {
        PyErr_Format(CrErr_Exception, "%s", tmp_err->message);
        g_clear_error(&tmp_err);
        return NULL;
    }

    Py_RETURN_NONE;
}

PyObject *
py_xml_parse_filelists(PyObject *self, PyObject *args)
{
    CR_UNUSED(self);

    char *filename;
    PyObject *py_newpkgcb, *py_pkgcb, *py_warningcb;
    CbData cbdata;
    GError *tmp_err = NULL;

    if (!PyArg_ParseTuple(args, "sOOO:py_xml_parse_filelists",
                                         &filename,
                                         &py_newpkgcb,
                                         &py_pkgcb,
                                         &py_warningcb)) {
        return NULL;
    }

    if (!PyCallable_Check(py_newpkgcb)) {
        PyErr_SetString(PyExc_TypeError, "newpkgcb must be callable");
        return NULL;
    }

    if (!PyCallable_Check(py_pkgcb) && py_pkgcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "pkgcb must be callable or None");
        return NULL;
    }

    if (!PyCallable_Check(py_warningcb) && py_warningcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "warningcb must be callable or None");
        return NULL;
    }

    Py_XINCREF(py_newpkgcb);
    Py_XINCREF(py_pkgcb);
    Py_XINCREF(py_warningcb);

    cr_XmlParserPkgCb       ptr_c_pkgcb     = NULL;
    cr_XmlParserWarningCb   ptr_c_warningcb = NULL;

    if (py_pkgcb != Py_None)
        ptr_c_pkgcb = c_pkgcb;
    if (py_warningcb != Py_None)
        ptr_c_warningcb = c_warningcb;

    cbdata.py_newpkgcb  = py_newpkgcb;
    cbdata.py_pkgcb     = py_pkgcb;
    cbdata.py_warningcb = py_warningcb;
    cbdata.py_pkg       = NULL;

    cr_xml_parse_filelists(filename,
                           c_newpkgcb,
                           &cbdata,
                           ptr_c_pkgcb,
                           &cbdata,
                           ptr_c_warningcb,
                           &cbdata,
                           &tmp_err);

    Py_XDECREF(py_newpkgcb);
    Py_XDECREF(py_pkgcb);
    Py_XDECREF(py_warningcb);

    if (tmp_err) {
        PyErr_Format(CrErr_Exception, "%s", tmp_err->message);
        g_clear_error(&tmp_err);
        return NULL;
    }

    Py_RETURN_NONE;
}

PyObject *
py_xml_parse_other(PyObject *self, PyObject *args)
{
    CR_UNUSED(self);

    char *filename;
    PyObject *py_newpkgcb, *py_pkgcb, *py_warningcb;
    CbData cbdata;
    GError *tmp_err = NULL;

    if (!PyArg_ParseTuple(args, "sOOO:py_xml_parse_other",
                                         &filename,
                                         &py_newpkgcb,
                                         &py_pkgcb,
                                         &py_warningcb)) {
        return NULL;
    }

    if (!PyCallable_Check(py_newpkgcb)) {
        PyErr_SetString(PyExc_TypeError, "newpkgcb must be callable");
        return NULL;
    }

    if (!PyCallable_Check(py_pkgcb) && py_pkgcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "pkgcb must be callable or None");
        return NULL;
    }

    if (!PyCallable_Check(py_warningcb) && py_warningcb != Py_None) {
        PyErr_SetString(PyExc_TypeError, "warningcb must be callable or None");
        return NULL;
    }

    Py_XINCREF(py_newpkgcb);
    Py_XINCREF(py_pkgcb);
    Py_XINCREF(py_warningcb);

    cr_XmlParserPkgCb       ptr_c_pkgcb     = NULL;
    cr_XmlParserWarningCb   ptr_c_warningcb = NULL;

    if (py_pkgcb != Py_None)
        ptr_c_pkgcb = c_pkgcb;
    if (py_warningcb != Py_None)
        ptr_c_warningcb = c_warningcb;

    cbdata.py_newpkgcb  = py_newpkgcb;
    cbdata.py_pkgcb     = py_pkgcb;
    cbdata.py_warningcb = py_warningcb;
    cbdata.py_pkg       = NULL;

    cr_xml_parse_other(filename,
                       c_newpkgcb,
                       &cbdata,
                       ptr_c_pkgcb,
                       &cbdata,
                       ptr_c_warningcb,
                       &cbdata,
                       &tmp_err);

    Py_XDECREF(py_newpkgcb);
    Py_XDECREF(py_pkgcb);
    Py_XDECREF(py_warningcb);

    if (tmp_err) {
        PyErr_Format(CrErr_Exception, "%s", tmp_err->message);
        g_clear_error(&tmp_err);
        return NULL;
    }

    Py_RETURN_NONE;
}