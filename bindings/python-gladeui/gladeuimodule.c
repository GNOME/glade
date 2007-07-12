
#include "config.h"

#include <pygobject.h>
 
void gladeui_register_classes (PyObject *d);
void gladeui_add_constants(PyObject *module, const gchar *strip_prefix);
extern PyMethodDef gladeui_functions[];
 
DL_EXPORT(void)
init_gladeui(void)
{
	PyObject *m, *d;
 
	init_pygobject ();
 
	m = Py_InitModule ("_gladeui", gladeui_functions);

	gladeui_dict = d = PyModule_GetDict (m); 
 
	gladeui_register_classes (d);
	gladeui_add_constants (m, "GLADE_");
 
	if (PyErr_Occurred ()) {
		Py_FatalError ("can't initialise module gladeui");
	}
}
