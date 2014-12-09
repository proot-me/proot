#include <Python.h>
#include "extension/extension.h"
#include "cli/note.h"

static PyObject *python_callback_func;

/* List of syscalls handled by this extensions.  */
static FilteredSysnum filtered_sysnums[] = {
	FILTERED_SYSNUM_END,
};

/* build by swig */
extern void init_proot(void);

/* helper for proot module */
Tracee *get_tracee_from_extension(long extension_handle)
{
	Extension *extension = (Extension *)extension_handle;
	Tracee *tracee = TRACEE(extension);

	return tracee;
}

/* init python once */
void init_python_env()
{
	static bool is_done = false;

	if (!is_done) {
		PyObject *pName, *pModule;

		Py_Initialize();
		init_proot();
		PyRun_SimpleString("import sys");
		PyRun_SimpleString("sys.path.insert(0, '/home/mike/work/PRoot/src/extension/python')");
		pName = PyString_FromString("python_extension");
		if (pName) {
			pModule = PyImport_Import(pName);
			Py_DECREF(pName);
			if (pModule) {
				python_callback_func = PyObject_GetAttrString(pModule, "python_callback");
				if (python_callback_func && PyCallable_Check(python_callback_func))
					note(NULL, INFO, USER, "python_callback find\n");
				else
					note(NULL, ERROR, USER, "python_callback_func error\n");
			} else {
				PyErr_Print();
				note(NULL, ERROR, USER, "pModule error\n");
			}
		} else
			note(NULL, ERROR, USER, "pName error\n");
		is_done = true;
	}
}

/* call python callback */
static int python_callback_func_wrapper(Extension *extension, ExtensionEvent event, intptr_t data1, intptr_t data2)
{
	int res = 0;
	PyObject *pArgs;
	PyObject *pValue;

	pArgs = PyTuple_New(4);
	if (pArgs) {
		/* setargs */
		pValue = PyLong_FromLong((long) extension);
		if (!pValue)
			note(NULL, ERROR, USER, "pValue allocation failure\n");
		PyTuple_SetItem(pArgs, 0, pValue);
		
		pValue = PyInt_FromLong(event);
		if (!pValue)
			note(NULL, ERROR, USER, "pValue allocation failure\n");
		PyTuple_SetItem(pArgs, 1, pValue);
		
		pValue = PyInt_FromLong(data1);
		if (!pValue)
			note(NULL, ERROR, USER, "pValue allocation failure\n");
		PyTuple_SetItem(pArgs, 2, pValue);
		
		pValue = PyInt_FromLong(data2);
		if (!pValue)
			note(NULL, ERROR, USER, "pValue allocation failure\n");
		PyTuple_SetItem(pArgs, 3, pValue);

		/* call function */
		pValue = PyObject_CallObject(python_callback_func, pArgs);
		if (pValue != NULL) {
			res = PyInt_AsLong(pValue);
			Py_DECREF(pValue);
		} else {
			PyErr_Print();
			note(NULL, ERROR, USER, "fail to call callback\n");
		}
		Py_DECREF(pArgs);
	} else
		note(NULL, ERROR, USER, "pArgs allocation failure\n");

	return res;
}

/**
 * Handler for this @extension.  It is triggered each time an @event
 * occurred.  See ExtensionEvent for the meaning of @data1 and @data2.
 */
int python_callback(Extension *extension, ExtensionEvent event, intptr_t data1, intptr_t data2)
{
	int res = 0;

	switch (event) {
		case INITIALIZATION:
			{
				init_python_env();
				res = python_callback_func_wrapper(extension, event, data1, data2);

				extension->filtered_sysnums = filtered_sysnums;
			}
			break;
		default:
			res = python_callback_func_wrapper(extension, event, data1, data2);
	}

	return res;
}
