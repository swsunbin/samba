/*
 * Unix SMB/CIFS implementation.
 * Copyright (C) Volker Lendecke 2022
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "lib/replace/system/python.h"
#include "replace.h"
#include "python/modules.h"
#include "python/py3compat.h"
#include "libcli/util/pyerrors.h"
#include "reparse.h"
#include "lib/util/iov_buf.h"
#include "smb_constants.h"

static PyObject *py_reparse_put(PyObject *module, PyObject *args)
{
	char *reparse = NULL;
	Py_ssize_t reparse_len;
	unsigned long long tag = 0;
	unsigned reserved = 0;
	uint8_t *buf = NULL;
	ssize_t buflen;
	PyObject *result = NULL;
	struct reparse_data_buffer reparse_buf = {};
	bool ok;

	ok = PyArg_ParseTuple(
		args,
		"Kk"PYARG_BYTES_LEN":put",
		&tag,
		&reserved,
		&reparse,
		&reparse_len);
	if (!ok) {
		return NULL;
	}

	reparse_buf.tag = tag;
	reparse_buf.parsed.raw.data = (uint8_t *)reparse;
	reparse_buf.parsed.raw.length = reparse_len;
	reparse_buf.parsed.raw.reserved = reserved;

	buflen = reparse_data_buffer_marshall(&reparse_buf, NULL, 0);
	if (buflen == -1) {
		errno = EINVAL;
		PyErr_SetFromErrno(PyExc_RuntimeError);
		return NULL;
	}
	buf = talloc_array(NULL, uint8_t, buflen);
	if (buf == NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	reparse_data_buffer_marshall(&reparse_buf, buf, buflen);

	result = PyBytes_FromStringAndSize((char *)buf, buflen);
	TALLOC_FREE(buf);
	return result;
}

static PyObject *py_reparse_symlink_put(PyObject *module, PyObject *args)
{
	int unparsed = 0;
	int flags = 0;
	struct reparse_data_buffer reparse = {
		.tag = IO_REPARSE_TAG_SYMLINK,
	};
	struct symlink_reparse_struct *lnk = &reparse.parsed.lnk;
	uint8_t stackbuf[1024];
	uint8_t *buf = stackbuf;
	ssize_t buflen = sizeof(stackbuf);
	PyObject *result = NULL;
	bool ok;

	ok = PyArg_ParseTuple(args,
			      "ssii:symlink_put",
			      &lnk->substitute_name,
			      &lnk->print_name,
			      &unparsed,
			      &flags);
	if (!ok) {
		return NULL;
	}
	lnk->unparsed_path_length = unparsed;
	lnk->flags = flags;

	buflen = reparse_data_buffer_marshall(&reparse, buf, buflen);

	if ((buflen > 0) && ((size_t)buflen > sizeof(stackbuf))) {
		buf = malloc(buflen);
		buflen = reparse_data_buffer_marshall(&reparse, buf, buflen);
	}

	if (buflen == -1) {
		PyErr_NoMemory();
	} else {
		result = PyBytes_FromStringAndSize((char *)buf, buflen);
	}

	if (buf != stackbuf) {
		free(buf);
	}

	return result;
}

static PyObject *py_reparse_symlink_get(PyObject *module, PyObject *args)
{
	char *buf = NULL;
	Py_ssize_t buflen;
	struct reparse_data_buffer *syml = NULL;
	struct symlink_reparse_struct *lnk = NULL;
	PyObject *result = NULL;
	NTSTATUS status;
	bool ok;

	ok = PyArg_ParseTuple(args, PYARG_BYTES_LEN ":get", &buf, &buflen);
	if (!ok) {
		return NULL;
	}

	syml = talloc(NULL, struct reparse_data_buffer);
	if (syml == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	status = reparse_data_buffer_parse(syml, syml, (uint8_t *)buf, buflen);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(syml);
		PyErr_SetNTSTATUS(status);
		return NULL;
	}

	if (syml->tag != IO_REPARSE_TAG_SYMLINK) {
		TALLOC_FREE(syml);
		PyErr_SetNTSTATUS(NT_STATUS_INVALID_NETWORK_RESPONSE);
		return NULL;
	}
	lnk = &syml->parsed.lnk;

	result = Py_BuildValue("ssII",
			       lnk->substitute_name,
			       lnk->print_name,
			       (unsigned)lnk->unparsed_path_length,
			       (unsigned)lnk->flags);

	TALLOC_FREE(syml);
	return result;
}

static PyMethodDef py_reparse_symlink_methods[] = {
	{ "put",
	  PY_DISCARD_FUNC_SIG(PyCFunction, py_reparse_put),
	  METH_VARARGS,
	  "Create a reparse point blob"},
	{ "symlink_put",
	  PY_DISCARD_FUNC_SIG(PyCFunction, py_reparse_symlink_put),
	  METH_VARARGS,
	  "Create a reparse symlink blob"},
	{ "symlink_get",
	  PY_DISCARD_FUNC_SIG(PyCFunction, py_reparse_symlink_get),
	  METH_VARARGS,
	  "Parse a reparse symlink blob"},
	{0},
};

static struct PyModuleDef moduledef = {
	PyModuleDef_HEAD_INIT,
	.m_name = "reparse_symlink",
	.m_doc = "[un]marshall reparse symlink blobs",
	.m_size = -1,
	.m_methods = py_reparse_symlink_methods,
};

MODULE_INIT_FUNC(reparse_symlink)
{
	PyObject *m;

	m = PyModule_Create(&moduledef);
	if (m == NULL)
		return NULL;

	return m;
}
