/*
   Unix SMB/CIFS implementation.

   Python interface to ldb - utility functions.

   Copyright (C) 2007-2010 Jelmer Vernooij <jelmer@samba.org>

	 ** NOTE! The following LGPL license applies to the ldb
	 ** library. This does NOT imply that all of Samba is released
	 ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include "lib/replace/system/python.h"
#include "ldb.h"
#include "pyldb.h"

static PyObject *ldb_module = NULL;

/**
 * Find out PyTypeObject in ldb module for a given typename
 */
static PyTypeObject * PyLdb_GetPyType(const char *typename)
{
	PyTypeObject *type = NULL;
	bool ok;

	if (ldb_module == NULL) {
		ldb_module = PyImport_ImportModule("ldb");
		if (ldb_module == NULL) {
			return NULL;
		}
	}

	type = (PyTypeObject *)PyObject_GetAttrString(ldb_module, typename);


	if (type == NULL) {
		PyErr_Format(PyExc_NameError,
			     "Unable to find type %s in ldb module",
			     typename);
		return NULL;
	}

	ok = PyType_Check(type);
	if (! ok) {
		PyErr_Format(PyExc_TypeError,
			     "Expected type ldb.%s, not %s",
			     typename, Py_TYPE(type)->tp_name);
		Py_DECREF(type);
		return NULL;
	}

	return type;
}

bool pyldb_check_type(PyObject *obj, const char *typename)
{
	bool ok = false;
	PyTypeObject *type = PyLdb_GetPyType(typename);
	if (type != NULL) {
		ok = PyObject_TypeCheck(obj, type);
		Py_DECREF(type);
	}
	return ok;
}

/**
 * Obtain a ldb DN from a Python object.
 *
 * @param mem_ctx Memory context
 * @param object Python object
 * @param ldb_ctx LDB context
 * @return Whether or not the conversion succeeded
 */
bool pyldb_Object_AsDn(TALLOC_CTX *mem_ctx, PyObject *object, 
		   struct ldb_context *ldb_ctx, struct ldb_dn **dn)
{
	struct ldb_dn *odn;
	PyTypeObject *PyLdb_Dn_Type;
	bool is_dn;

	if (ldb_ctx != NULL && (PyUnicode_Check(object))) {
		const char *odn_str = NULL;

		odn_str = PyUnicode_AsUTF8(object);
		if (odn_str == NULL) {
			return false;
		}

		odn = ldb_dn_new(mem_ctx, ldb_ctx, odn_str);
		if (odn == NULL) {
			PyErr_NoMemory();
			return false;
		}

		*dn = odn;
		return true;
	}

	if (ldb_ctx != NULL && PyBytes_Check(object)) {
		const char *odn_str = NULL;

		odn_str = PyBytes_AsString(object);
		if (odn_str == NULL) {
			return false;
		}

		odn = ldb_dn_new(mem_ctx, ldb_ctx, odn_str);
		if (odn == NULL) {
			PyErr_NoMemory();
			return false;
		}

		*dn = odn;
		return true;
	}

	PyLdb_Dn_Type = PyLdb_GetPyType("Dn");
	if (PyLdb_Dn_Type == NULL) {
		return false;
	}

	is_dn = PyObject_TypeCheck(object, PyLdb_Dn_Type);
	Py_DECREF(PyLdb_Dn_Type);
	if (is_dn) {
		*dn = pyldb_Dn_AS_DN(object);
		return true;
	}

	PyErr_SetString(PyExc_TypeError, "Expected DN");
	return false;
}

PyObject *pyldb_Dn_FromDn(struct ldb_dn *dn)
{
	TALLOC_CTX *mem_ctx = NULL;
	struct ldb_dn *dn_ref = NULL;
	PyLdbDnObject *py_ret;
	PyTypeObject *PyLdb_Dn_Type;

	if (dn == NULL) {
		Py_RETURN_NONE;
	}

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		return PyErr_NoMemory();
	}

	dn_ref = talloc_reference(mem_ctx, dn);
	if (dn_ref == NULL) {
		talloc_free(mem_ctx);
		return PyErr_NoMemory();
	}

	PyLdb_Dn_Type = PyLdb_GetPyType("Dn");
	if (PyLdb_Dn_Type == NULL) {
		talloc_free(mem_ctx);
		return NULL;
	}

	py_ret = (PyLdbDnObject *)PyLdb_Dn_Type->tp_alloc(PyLdb_Dn_Type, 0);
	Py_DECREF(PyLdb_Dn_Type);
	if (py_ret == NULL) {
		talloc_free(mem_ctx);
		PyErr_NoMemory();
		return NULL;
	}
	py_ret->mem_ctx = mem_ctx;
	py_ret->dn = dn;
	return (PyObject *)py_ret;
}
