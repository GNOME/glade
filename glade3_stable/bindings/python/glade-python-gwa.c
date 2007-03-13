/* -- THIS FILE IS GENERATED - DO NOT EDIT *//* -*- Mode: C; c-basic-offset: 4 -*- */

#include <Python.h>



#line 23 "glade-python-gwa.override"

#include <Python.h>
#define NO_IMPORT_PYGOBJECT
#include <pygobject.h>

#include <gladeui/glade.h>

static gboolean
glade_python_support_init_value (GObject *object,
				 gboolean is_pack,
				 const gchar *property_name,
				 GValue *value)
{
	GladeWidget *widget;
	GladeProperty *property;
	
	if ((widget = glade_widget_get_from_gobject (object)) == NULL)
	{
		PyErr_Format (PyExc_AssertionError, "GladeWidget != NULL");
		return FALSE;
	}
	
	if (is_pack)
		property = glade_widget_get_pack_property (widget, property_name);
	else
		property = glade_widget_get_property (widget, property_name);
	
	if (property == NULL)
	{
        	PyErr_Format (PyExc_TypeError, "'%s' does not support %s property `%s'",
			      glade_widget_get_name (widget),
			      (is_pack) ? "packing" : "",
			      property_name);
		return FALSE;
	}
	
	g_value_init (value, G_VALUE_TYPE (property->value));
	return TRUE;
}


static GList *
glade_python_support_glist_from_list (PyObject *list)
{
	gint i, size = PyList_Size (list);
	GList *retval = NULL;
	
	for (i = 0; i < size; i++)
	{
		PyObject *val = PyList_GetItem (list, i);
		retval = g_list_prepend (retval, pygobject_get(val));
	}
	return retval;
}

static PyObject *
glade_python_support_list_from_glist (GList *list)
{
	PyObject *retval = PyList_New (0);;
	
	for (; list; list = g_list_next (list))
	{
		GObject *obj = list->data;
		PyList_Append (retval, Py_BuildValue ("O", pygobject_new (obj)));
	}
	
	return retval;
}

/*
  NOTE: the next functions where created by codegen and adapted to support 
  GValue* and GList* types.
*/

#line 83 "glade-python-gwa.c"


/* ---------- types from other modules ---------- */
static PyTypeObject *_PyGObject_Type;
#define PyGObject_Type (*_PyGObject_Type)


/* ---------- forward type declarations ---------- */
PyTypeObject PyGladeWidgetAdaptor_Type;

#line 94 "glade-python-gwa.c"



/* ----------- GladeWidgetAdaptor ----------- */

#line 101 "glade-python-gwa.override"
static PyObject *
_wrap_GladeWidgetAdaptor__do_verify_property(PyObject *cls, PyObject *args, PyObject *kwargs)
{
    gpointer klass;
    static char *kwlist[] = { "self", "object", "property_name", "value", NULL };
    PyGObject *self, *object;
    char *property_name;
    int ret;
    GValue value = {0,};
    PyObject *val;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!O!sO:GladeWidgetAdaptor.verify_property", kwlist, &PyGladeWidgetAdaptor_Type, &self, &PyGObject_Type, &object, &property_name, &val))
        return NULL;
    klass = g_type_class_ref(pyg_type_from_object(cls));
    
    glade_python_support_init_value (object->obj, FALSE, property_name, &value);
    pyg_value_from_pyobject (&value, val);
    
    if (GLADE_WIDGET_ADAPTOR_CLASS(klass)->verify_property)
        ret = GLADE_WIDGET_ADAPTOR_CLASS(klass)->verify_property(GLADE_WIDGET_ADAPTOR(self->obj), G_OBJECT(object->obj), property_name, &value);
    else {
        PyErr_SetString(PyExc_NotImplementedError, "virtual method GladeWidgetAdaptor.verify_property not implemented");
        g_type_class_unref(klass);
        return NULL;
    }
    g_type_class_unref(klass);
    return PyBool_FromLong(ret);

}
#line 130 "glade-python-gwa.c"


#line 215 "glade-python-gwa.override"
static PyObject *
_wrap_GladeWidgetAdaptor__do_set_property(PyObject *cls, PyObject *args, PyObject *kwargs)
{
    gpointer klass;
    static char *kwlist[] = { "self", "object", "property_name", "value", NULL };
    PyGObject *self, *object;
    char *property_name;
    GValue value = {0,};
    PyObject *val;
    
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!O!sO:GladeWidgetAdaptor.set_property", kwlist, &PyGladeWidgetAdaptor_Type, &self, &PyGObject_Type, &object, &property_name, &val))
        return NULL;
    klass = g_type_class_ref(pyg_type_from_object(cls));
    
    glade_python_support_init_value (object->obj, FALSE, property_name, &value);
    pyg_value_from_pyobject (&value, val);
    
    if (GLADE_WIDGET_ADAPTOR_CLASS(klass)->set_property)
        GLADE_WIDGET_ADAPTOR_CLASS(klass)->set_property(GLADE_WIDGET_ADAPTOR(self->obj), G_OBJECT(object->obj), property_name, &value);
    else {
        PyErr_SetString(PyExc_NotImplementedError, "virtual method GladeWidgetAdaptor.set_property not implemented");
        g_type_class_unref(klass);
        return NULL;
    }
    g_type_class_unref(klass);
    Py_INCREF(Py_None);
    return Py_None;
}
#line 162 "glade-python-gwa.c"


#line 335 "glade-python-gwa.override"
static PyObject *
_wrap_GladeWidgetAdaptor__do_get_property(PyObject *cls, PyObject *args, PyObject *kwargs)
{
    gpointer klass;
    static char *kwlist[] = { "self", "object", "property_name", NULL };
    PyGObject *self, *object;
    char *property_name;
    GValue value = {0,};
    PyObject *ret;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!O!s:GladeWidgetAdaptor.get_property", kwlist, &PyGladeWidgetAdaptor_Type, &self, &PyGObject_Type, &object, &property_name))
        return NULL;
    klass = g_type_class_ref(pyg_type_from_object(cls));
    
    if (GLADE_WIDGET_ADAPTOR_CLASS(klass)->get_property)
    {
	glade_python_support_init_value (object->obj, FALSE, property_name, &value);
        GLADE_WIDGET_ADAPTOR_CLASS(klass)->get_property(GLADE_WIDGET_ADAPTOR(self->obj), G_OBJECT(object->obj), property_name, &value);
	ret = pyg_value_as_pyobject (&value, TRUE);
	g_value_unset (&value);
    }
    else {
        PyErr_SetString(PyExc_NotImplementedError, "virtual method GladeWidgetAdaptor.get_property not implemented");
        g_type_class_unref(klass);
        return NULL;
    }
    g_type_class_unref(klass);
    return ret;
}
#line 195 "glade-python-gwa.c"


#line 435 "glade-python-gwa.override"
static PyObject *
_wrap_GladeWidgetAdaptor__do_child_verify_property(PyObject *cls, PyObject *args, PyObject *kwargs)
{
    gpointer klass;
    static char *kwlist[] = { "self", "container", "child", "property_name", "value", NULL };
    PyGObject *self, *container, *child;
    char *property_name;
    int ret;
    GValue value = {0,};
    PyObject *val;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!O!O!sO:GladeWidgetAdaptor.child_verify_property", kwlist, &PyGladeWidgetAdaptor_Type, &self, &PyGObject_Type, &container, &PyGObject_Type, &child, &property_name, &val))
        return NULL;
    klass = g_type_class_ref(pyg_type_from_object(cls));
    
    glade_python_support_init_value (child->obj, TRUE, property_name, &value);
    pyg_value_from_pyobject (&value, val);
    
    if (GLADE_WIDGET_ADAPTOR_CLASS(klass)->child_verify_property)
        ret = GLADE_WIDGET_ADAPTOR_CLASS(klass)->child_verify_property(GLADE_WIDGET_ADAPTOR(self->obj), G_OBJECT(container->obj), G_OBJECT(child->obj), property_name, &value);
    else {
        PyErr_SetString(PyExc_NotImplementedError, "virtual method GladeWidgetAdaptor.child_verify_property not implemented");
        g_type_class_unref(klass);
        return NULL;
    }
    g_type_class_unref(klass);
    return PyBool_FromLong(ret);

}
#line 228 "glade-python-gwa.c"


#line 559 "glade-python-gwa.override"
static PyObject *
_wrap_GladeWidgetAdaptor__do_child_set_property(PyObject *cls, PyObject *args, PyObject *kwargs)
{
    gpointer klass;
    static char *kwlist[] = { "self", "container", "child", "property_name", "value", NULL };
    PyGObject *self, *container, *child;
    char *property_name;
    GValue value = {0,};
    PyObject *val;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!O!O!sO:GladeWidgetAdaptor.child_set_property", kwlist, &PyGladeWidgetAdaptor_Type, &self, &PyGObject_Type, &container, &PyGObject_Type, &child, &property_name, &val))
        return NULL;
    klass = g_type_class_ref(pyg_type_from_object(cls));
    
    glade_python_support_init_value (child->obj, TRUE, property_name, &value);
    pyg_value_from_pyobject (&value, val);
    
    if (GLADE_WIDGET_ADAPTOR_CLASS(klass)->child_set_property)
        GLADE_WIDGET_ADAPTOR_CLASS(klass)->child_set_property(GLADE_WIDGET_ADAPTOR(self->obj), G_OBJECT(container->obj), G_OBJECT(child->obj), property_name, &value);
    else {
        PyErr_SetString(PyExc_NotImplementedError, "virtual method GladeWidgetAdaptor.child_set_property not implemented");
        g_type_class_unref(klass);
        return NULL;
    }
    g_type_class_unref(klass);
    Py_INCREF(Py_None);
    return Py_None;
}
#line 260 "glade-python-gwa.c"


#line 689 "glade-python-gwa.override"
static PyObject *
_wrap_GladeWidgetAdaptor__do_child_get_property(PyObject *cls, PyObject *args, PyObject *kwargs)
{
    gpointer klass;
    static char *kwlist[] = { "self", "container", "child", "property_name", NULL };
    PyGObject *self, *container, *child;
    char *property_name;
    PyObject *ret;
    GValue value = { 0, } ;
    
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!O!O!s:GladeWidgetAdaptor.child_get_property", kwlist, &PyGladeWidgetAdaptor_Type, &self, &PyGObject_Type, &container, &PyGObject_Type, &child, &property_name))
        return NULL;
    klass = g_type_class_ref(pyg_type_from_object(cls));
    if (GLADE_WIDGET_ADAPTOR_CLASS(klass)->child_get_property)
    {
	glade_python_support_init_value (child->obj, TRUE, property_name, &value);
        GLADE_WIDGET_ADAPTOR_CLASS(klass)->child_get_property(GLADE_WIDGET_ADAPTOR(self->obj), G_OBJECT(container->obj), G_OBJECT(child->obj), property_name, &value);
	ret = pyg_value_as_pyobject (&value, TRUE);
	g_value_unset (&value);
    }
    else {
        PyErr_SetString(PyExc_NotImplementedError, "virtual method GladeWidgetAdaptor.child_get_property not implemented");
        g_type_class_unref(klass);
        return NULL;
    }
    g_type_class_unref(klass);
    return ret;
}
#line 292 "glade-python-gwa.c"


#line 797 "glade-python-gwa.override"
static PyObject *
_wrap_GladeWidgetAdaptor__do_get_children(PyObject *cls, PyObject *args, PyObject *kwargs)
{
    gpointer klass;
    static char *kwlist[] = { "self", "container", NULL };
    PyGObject *self, *container;
    PyObject *list;
    GList *ret;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!O!:GladeWidgetAdaptor.get_children", kwlist, &PyGladeWidgetAdaptor_Type, &self, &PyGObject_Type, &container))
        return NULL;
    klass = g_type_class_ref(pyg_type_from_object(cls));
    if (GLADE_WIDGET_ADAPTOR_CLASS(klass)->get_children)
        ret = GLADE_WIDGET_ADAPTOR_CLASS(klass)->get_children(GLADE_WIDGET_ADAPTOR(self->obj), G_OBJECT(container->obj));
    else {
        PyErr_SetString(PyExc_NotImplementedError, "virtual method GladeWidgetAdaptor.get_children not implemented");
        g_type_class_unref(klass);
        return NULL;
    }
    g_type_class_unref(klass);

    list = glade_python_support_list_from_glist (ret);
    g_list_free (ret);
    return list;
}
#line 321 "glade-python-gwa.c"


static PyObject *
_wrap_GladeWidgetAdaptor__do_add(PyObject *cls, PyObject *args, PyObject *kwargs)
{
    gpointer klass;
    static char *kwlist[] = { "self", "parent", "child", NULL };
    PyGObject *self, *parent, *child;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!O!O!:GladeWidgetAdaptor.add", kwlist, &PyGladeWidgetAdaptor_Type, &self, &PyGObject_Type, &parent, &PyGObject_Type, &child))
        return NULL;
    klass = g_type_class_ref(pyg_type_from_object(cls));
    if (GLADE_WIDGET_ADAPTOR_CLASS(klass)->add)
        GLADE_WIDGET_ADAPTOR_CLASS(klass)->add(GLADE_WIDGET_ADAPTOR(self->obj), G_OBJECT(parent->obj), G_OBJECT(child->obj));
    else {
        PyErr_SetString(PyExc_NotImplementedError, "virtual method GladeWidgetAdaptor.add not implemented");
        g_type_class_unref(klass);
        return NULL;
    }
    g_type_class_unref(klass);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
_wrap_GladeWidgetAdaptor__do_remove(PyObject *cls, PyObject *args, PyObject *kwargs)
{
    gpointer klass;
    static char *kwlist[] = { "self", "parent", "child", NULL };
    PyGObject *self, *parent, *child;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!O!O!:GladeWidgetAdaptor.remove", kwlist, &PyGladeWidgetAdaptor_Type, &self, &PyGObject_Type, &parent, &PyGObject_Type, &child))
        return NULL;
    klass = g_type_class_ref(pyg_type_from_object(cls));
    if (GLADE_WIDGET_ADAPTOR_CLASS(klass)->remove)
        GLADE_WIDGET_ADAPTOR_CLASS(klass)->remove(GLADE_WIDGET_ADAPTOR(self->obj), G_OBJECT(parent->obj), G_OBJECT(child->obj));
    else {
        PyErr_SetString(PyExc_NotImplementedError, "virtual method GladeWidgetAdaptor.remove not implemented");
        g_type_class_unref(klass);
        return NULL;
    }
    g_type_class_unref(klass);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
_wrap_GladeWidgetAdaptor__do_replace_child(PyObject *cls, PyObject *args, PyObject *kwargs)
{
    gpointer klass;
    static char *kwlist[] = { "self", "container", "old", "new", NULL };
    PyGObject *self, *container, *old, *new;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!O!O!O!:GladeWidgetAdaptor.replace_child", kwlist, &PyGladeWidgetAdaptor_Type, &self, &PyGObject_Type, &container, &PyGObject_Type, &old, &PyGObject_Type, &new))
        return NULL;
    klass = g_type_class_ref(pyg_type_from_object(cls));
    if (GLADE_WIDGET_ADAPTOR_CLASS(klass)->replace_child)
        GLADE_WIDGET_ADAPTOR_CLASS(klass)->replace_child(GLADE_WIDGET_ADAPTOR(self->obj), G_OBJECT(container->obj), G_OBJECT(old->obj), G_OBJECT(new->obj));
    else {
        PyErr_SetString(PyExc_NotImplementedError, "virtual method GladeWidgetAdaptor.replace_child not implemented");
        g_type_class_unref(klass);
        return NULL;
    }
    g_type_class_unref(klass);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
_wrap_GladeWidgetAdaptor__do_post_create(PyObject *cls, PyObject *args, PyObject *kwargs)
{
    gpointer klass;
    static char *kwlist[] = { "self", "object", "reason", NULL };
    PyGObject *self, *object;
    PyObject *py_reason = NULL;
    GladeCreateReason reason;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!O!O:GladeWidgetAdaptor.post_create", kwlist, &PyGladeWidgetAdaptor_Type, &self, &PyGObject_Type, &object, &py_reason))
        return NULL;
    if (pyg_enum_get_value(GLADE_CREATE_REASON, py_reason, (gint *)&reason))
        return NULL;
    klass = g_type_class_ref(pyg_type_from_object(cls));
    if (GLADE_WIDGET_ADAPTOR_CLASS(klass)->post_create)
        GLADE_WIDGET_ADAPTOR_CLASS(klass)->post_create(GLADE_WIDGET_ADAPTOR(self->obj), G_OBJECT(object->obj), reason);
    else {
        PyErr_SetString(PyExc_NotImplementedError, "virtual method GladeWidgetAdaptor.post_create not implemented");
        g_type_class_unref(klass);
        return NULL;
    }
    g_type_class_unref(klass);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
_wrap_GladeWidgetAdaptor__do_get_internal_child(PyObject *cls, PyObject *args, PyObject *kwargs)
{
    gpointer klass;
    static char *kwlist[] = { "self", "parent", "name", NULL };
    PyGObject *self, *parent;
    char *name;
    GObject *ret;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!O!s:GladeWidgetAdaptor.get_internal_child", kwlist, &PyGladeWidgetAdaptor_Type, &self, &PyGObject_Type, &parent, &name))
        return NULL;
    klass = g_type_class_ref(pyg_type_from_object(cls));
    if (GLADE_WIDGET_ADAPTOR_CLASS(klass)->get_internal_child)
        ret = GLADE_WIDGET_ADAPTOR_CLASS(klass)->get_internal_child(GLADE_WIDGET_ADAPTOR(self->obj), G_OBJECT(parent->obj), name);
    else {
        PyErr_SetString(PyExc_NotImplementedError, "virtual method GladeWidgetAdaptor.get_internal_child not implemented");
        g_type_class_unref(klass);
        return NULL;
    }
    g_type_class_unref(klass);
    /* pygobject_new handles NULL checking */
    return pygobject_new((GObject *)ret);
}

static PyObject *
_wrap_GladeWidgetAdaptor__do_launch_editor(PyObject *cls, PyObject *args, PyObject *kwargs)
{
    gpointer klass;
    static char *kwlist[] = { "self", "object", NULL };
    PyGObject *self, *object;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!O!:GladeWidgetAdaptor.launch_editor", kwlist, &PyGladeWidgetAdaptor_Type, &self, &PyGObject_Type, &object))
        return NULL;
    klass = g_type_class_ref(pyg_type_from_object(cls));
    if (GLADE_WIDGET_ADAPTOR_CLASS(klass)->launch_editor)
        GLADE_WIDGET_ADAPTOR_CLASS(klass)->launch_editor(GLADE_WIDGET_ADAPTOR(self->obj), G_OBJECT(object->obj));
    else {
        PyErr_SetString(PyExc_NotImplementedError, "virtual method GladeWidgetAdaptor.launch_editor not implemented");
        g_type_class_unref(klass);
        return NULL;
    }
    g_type_class_unref(klass);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyMethodDef _PyGladeWidgetAdaptor_methods[] = {
    { "do_verify_property", (PyCFunction)_wrap_GladeWidgetAdaptor__do_verify_property, METH_VARARGS|METH_KEYWORDS|METH_CLASS },
    { "do_set_property", (PyCFunction)_wrap_GladeWidgetAdaptor__do_set_property, METH_VARARGS|METH_KEYWORDS|METH_CLASS },
    { "do_get_property", (PyCFunction)_wrap_GladeWidgetAdaptor__do_get_property, METH_VARARGS|METH_KEYWORDS|METH_CLASS },
    { "do_child_verify_property", (PyCFunction)_wrap_GladeWidgetAdaptor__do_child_verify_property, METH_VARARGS|METH_KEYWORDS|METH_CLASS },
    { "do_child_set_property", (PyCFunction)_wrap_GladeWidgetAdaptor__do_child_set_property, METH_VARARGS|METH_KEYWORDS|METH_CLASS },
    { "do_child_get_property", (PyCFunction)_wrap_GladeWidgetAdaptor__do_child_get_property, METH_VARARGS|METH_KEYWORDS|METH_CLASS },
    { "do_get_children", (PyCFunction)_wrap_GladeWidgetAdaptor__do_get_children, METH_VARARGS|METH_KEYWORDS|METH_CLASS },
    { "do_add", (PyCFunction)_wrap_GladeWidgetAdaptor__do_add, METH_VARARGS|METH_KEYWORDS|METH_CLASS },
    { "do_remove", (PyCFunction)_wrap_GladeWidgetAdaptor__do_remove, METH_VARARGS|METH_KEYWORDS|METH_CLASS },
    { "do_replace_child", (PyCFunction)_wrap_GladeWidgetAdaptor__do_replace_child, METH_VARARGS|METH_KEYWORDS|METH_CLASS },
    { "do_post_create", (PyCFunction)_wrap_GladeWidgetAdaptor__do_post_create, METH_VARARGS|METH_KEYWORDS|METH_CLASS },
    { "do_get_internal_child", (PyCFunction)_wrap_GladeWidgetAdaptor__do_get_internal_child, METH_VARARGS|METH_KEYWORDS|METH_CLASS },
    { "do_launch_editor", (PyCFunction)_wrap_GladeWidgetAdaptor__do_launch_editor, METH_VARARGS|METH_KEYWORDS|METH_CLASS },
    { NULL, NULL, 0 }
};

PyTypeObject PyGladeWidgetAdaptor_Type = {
    PyObject_HEAD_INIT(NULL)
    0,					/* ob_size */
    "GladeWidgetAdaptor",			/* tp_name */
    sizeof(PyGObject),	        /* tp_basicsize */
    0,					/* tp_itemsize */
    /* methods */
    (destructor)0,	/* tp_dealloc */
    (printfunc)0,			/* tp_print */
    (getattrfunc)0,	/* tp_getattr */
    (setattrfunc)0,	/* tp_setattr */
    (cmpfunc)0,		/* tp_compare */
    (reprfunc)0,		/* tp_repr */
    (PyNumberMethods*)0,     /* tp_as_number */
    (PySequenceMethods*)0, /* tp_as_sequence */
    (PyMappingMethods*)0,   /* tp_as_mapping */
    (hashfunc)0,		/* tp_hash */
    (ternaryfunc)0,		/* tp_call */
    (reprfunc)0,		/* tp_str */
    (getattrofunc)0,	/* tp_getattro */
    (setattrofunc)0,	/* tp_setattro */
    (PyBufferProcs*)0,	/* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,                      /* tp_flags */
    NULL, 				/* Documentation string */
    (traverseproc)0,	/* tp_traverse */
    (inquiry)0,		/* tp_clear */
    (richcmpfunc)0,	/* tp_richcompare */
    offsetof(PyGObject, weakreflist),             /* tp_weaklistoffset */
    (getiterfunc)0,		/* tp_iter */
    (iternextfunc)0,	/* tp_iternext */
    _PyGladeWidgetAdaptor_methods,			/* tp_methods */
    0,					/* tp_members */
    0,		       	/* tp_getset */
    NULL,				/* tp_base */
    NULL,				/* tp_dict */
    (descrgetfunc)0,	/* tp_descr_get */
    (descrsetfunc)0,	/* tp_descr_set */
    offsetof(PyGObject, inst_dict),                 /* tp_dictoffset */
    (initproc)0,		/* tp_init */
    (allocfunc)0,           /* tp_alloc */
    (newfunc)0,               /* tp_new */
    (freefunc)0,             /* tp_free */
    (inquiry)0              /* tp_is_gc */
};

#line 132 "glade-python-gwa.override"
static gboolean
_wrap_GladeWidgetAdaptor__proxy_do_verify_property(GladeWidgetAdaptor *self, GObject*object, const gchar*property_name, const GValue*value)
{
    PyGILState_STATE __py_state;
    PyObject *py_self;
    PyObject *py_object = NULL;
    PyObject *py_property_name;
    PyObject *py_value;
    gboolean retval;
    PyObject *py_retval;
    PyObject *py_args;
    PyObject *py_method;
    
    __py_state = pyg_gil_state_ensure();
    py_self = pygobject_new((GObject *) self);
    if (!py_self) {
        if (PyErr_Occurred())
            PyErr_Print();
        pyg_gil_state_release(__py_state);
        return FALSE;
    }
    if (object)
        py_object = pygobject_new((GObject *) object);
    else {
        Py_INCREF(Py_None);
        py_object = Py_None;
    }
    py_property_name = PyString_FromString(property_name);
    if (!py_property_name) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_object);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return FALSE;
    }
    py_value = pyg_value_as_pyobject(value, TRUE);
    if (!py_value) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_property_name);
        Py_DECREF(py_object);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return FALSE;
    }
    
    py_args = PyTuple_New(3);
    PyTuple_SET_ITEM(py_args, 0, py_object);
    PyTuple_SET_ITEM(py_args, 1, py_property_name);
    PyTuple_SET_ITEM(py_args, 2, py_value);
    
    py_method = PyObject_GetAttrString(py_self, "do_verify_property");
    if (!py_method) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return FALSE;
    }
    py_retval = PyObject_CallObject(py_method, py_args);
    if (!py_retval) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_method);
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return FALSE;
    }
    retval = PyObject_IsTrue(py_retval)? TRUE : FALSE;
    
    Py_DECREF(py_retval);
    Py_DECREF(py_method);
    Py_DECREF(py_args);
    Py_DECREF(py_self);
    pyg_gil_state_release(__py_state);
    
    return retval;
}
#line 606 "glade-python-gwa.c"


#line 245 "glade-python-gwa.override"
static void
_wrap_GladeWidgetAdaptor__proxy_do_set_property(GladeWidgetAdaptor *self, GObject*object, const gchar*property_name, const GValue*value)
{
    PyGILState_STATE __py_state;
    PyObject *py_self;
    PyObject *py_object = NULL;
    PyObject *py_property_name;
    PyObject *py_value;
    PyObject *py_retval;
    PyObject *py_args;
    PyObject *py_method;
    
    __py_state = pyg_gil_state_ensure();
    py_self = pygobject_new((GObject *) self);
    if (!py_self) {
        if (PyErr_Occurred())
            PyErr_Print();
        pyg_gil_state_release(__py_state);
        return;
    }
    if (object)
        py_object = pygobject_new((GObject *) object);
    else {
        Py_INCREF(Py_None);
        py_object = Py_None;
    }
    py_property_name = PyString_FromString(property_name);
    if (!py_property_name) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_object);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    py_value = pyg_value_as_pyobject(value, TRUE);
    if (!py_value) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_property_name);
        Py_DECREF(py_object);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    
    py_args = PyTuple_New(3);
    PyTuple_SET_ITEM(py_args, 0, py_object);
    PyTuple_SET_ITEM(py_args, 1, py_property_name);
    PyTuple_SET_ITEM(py_args, 2, py_value);
    
    py_method = PyObject_GetAttrString(py_self, "do_set_property");
    if (!py_method) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    py_retval = PyObject_CallObject(py_method, py_args);
    if (!py_retval) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_method);
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    if (py_retval != Py_None) {
        if (PyErr_Occurred())
            PyErr_Print();
        PyErr_SetString(PyExc_TypeError, "retval should be None");
        Py_DECREF(py_retval);
        Py_DECREF(py_method);
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    
    Py_DECREF(py_retval);
    Py_DECREF(py_method);
    Py_DECREF(py_args);
    Py_DECREF(py_self);
    pyg_gil_state_release(__py_state);
}
#line 698 "glade-python-gwa.c"


#line 366 "glade-python-gwa.override"
static void
_wrap_GladeWidgetAdaptor__proxy_do_get_property(GladeWidgetAdaptor *self, GObject*object, const gchar*property_name, GValue *value)
{
    PyGILState_STATE __py_state;
    PyObject *py_self;
    PyObject *py_object = NULL;
    PyObject *py_property_name;
    PyObject *py_retval;
    PyObject *py_args;
    PyObject *py_method;
    
    __py_state = pyg_gil_state_ensure();
    py_self = pygobject_new((GObject *) self);
    if (!py_self) {
        if (PyErr_Occurred())
            PyErr_Print();
        pyg_gil_state_release(__py_state);
        return;
    }
    if (object)
        py_object = pygobject_new((GObject *) object);
    else {
        Py_INCREF(Py_None);
        py_object = Py_None;
    }
    py_property_name = PyString_FromString(property_name);
    if (!py_property_name) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_object);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    
    py_args = PyTuple_New(3);
    PyTuple_SET_ITEM(py_args, 0, py_object);
    PyTuple_SET_ITEM(py_args, 1, py_property_name);
    
    py_method = PyObject_GetAttrString(py_self, "do_get_property");
    if (!py_method) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    py_retval = PyObject_CallObject(py_method, py_args);
    if (!py_retval) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_method);
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }

    pyg_value_from_pyobject (value, py_retval);
    
    Py_DECREF(py_retval);
    Py_DECREF(py_method);
    Py_DECREF(py_args);
    Py_DECREF(py_self);
    pyg_gil_state_release(__py_state);
}
#line 769 "glade-python-gwa.c"


#line 466 "glade-python-gwa.override"
static gboolean
_wrap_GladeWidgetAdaptor__proxy_do_child_verify_property(GladeWidgetAdaptor *self, GObject*container, GObject*child, const gchar*property_name, GValue*value)
{
    PyGILState_STATE __py_state;
    PyObject *py_self;
    PyObject *py_container = NULL;
    PyObject *py_child = NULL;
    PyObject *py_property_name;
    PyObject *py_value;
    gboolean retval;
    PyObject *py_retval;
    PyObject *py_args;
    PyObject *py_method;
    
    __py_state = pyg_gil_state_ensure();
    py_self = pygobject_new((GObject *) self);
    if (!py_self) {
        if (PyErr_Occurred())
            PyErr_Print();
        pyg_gil_state_release(__py_state);
        return FALSE;
    }
    if (container)
        py_container = pygobject_new((GObject *) container);
    else {
        Py_INCREF(Py_None);
        py_container = Py_None;
    }
    if (child)
        py_child = pygobject_new((GObject *) child);
    else {
        Py_INCREF(Py_None);
        py_child = Py_None;
    }
    py_property_name = PyString_FromString(property_name);
    if (!py_property_name) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_child);
        Py_DECREF(py_container);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return FALSE;
    }
    py_value = pyg_value_as_pyobject(value, TRUE);
    if (!py_value) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_property_name);
        Py_DECREF(py_child);
        Py_DECREF(py_container);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return FALSE;
    }
    
    py_args = PyTuple_New(4);
    PyTuple_SET_ITEM(py_args, 0, py_container);
    PyTuple_SET_ITEM(py_args, 1, py_child);
    PyTuple_SET_ITEM(py_args, 2, py_property_name);
    PyTuple_SET_ITEM(py_args, 3, py_value);
    
    py_method = PyObject_GetAttrString(py_self, "do_child_verify_property");
    if (!py_method) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return FALSE;
    }
    py_retval = PyObject_CallObject(py_method, py_args);
    if (!py_retval) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_method);
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return FALSE;
    }
    retval = PyObject_IsTrue(py_retval)? TRUE : FALSE;
    
    Py_DECREF(py_retval);
    Py_DECREF(py_method);
    Py_DECREF(py_args);
    Py_DECREF(py_self);
    pyg_gil_state_release(__py_state);
    
    return retval;
}
#line 864 "glade-python-gwa.c"


#line 589 "glade-python-gwa.override"
static void
_wrap_GladeWidgetAdaptor__proxy_do_child_set_property(GladeWidgetAdaptor *self, GObject*container, GObject*child, const gchar*property_name, const GValue *value)
{
    PyGILState_STATE __py_state;
    PyObject *py_self;
    PyObject *py_container = NULL;
    PyObject *py_child = NULL;
    PyObject *py_property_name;
    PyObject *py_value;
    PyObject *py_retval;
    PyObject *py_args;
    PyObject *py_method;
    
    __py_state = pyg_gil_state_ensure();
    py_self = pygobject_new((GObject *) self);
    if (!py_self) {
        if (PyErr_Occurred())
            PyErr_Print();
        pyg_gil_state_release(__py_state);
        return;
    }
    if (container)
        py_container = pygobject_new((GObject *) container);
    else {
        Py_INCREF(Py_None);
        py_container = Py_None;
    }
    if (child)
        py_child = pygobject_new((GObject *) child);
    else {
        Py_INCREF(Py_None);
        py_child = Py_None;
    }
    py_property_name = PyString_FromString(property_name);
    if (!py_property_name) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_child);
        Py_DECREF(py_container);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    py_value = pyg_value_as_pyobject (value, TRUE);
    if (!py_value) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_property_name);
        Py_DECREF(py_child);
        Py_DECREF(py_container);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    
    py_args = PyTuple_New(4);
    PyTuple_SET_ITEM(py_args, 0, py_container);
    PyTuple_SET_ITEM(py_args, 1, py_child);
    PyTuple_SET_ITEM(py_args, 2, py_property_name);
    PyTuple_SET_ITEM(py_args, 3, py_value);
    
    py_method = PyObject_GetAttrString(py_self, "do_child_set_property");
    if (!py_method) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    py_retval = PyObject_CallObject(py_method, py_args);
    if (!py_retval) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_method);
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    if (py_retval != Py_None) {
        if (PyErr_Occurred())
            PyErr_Print();
        PyErr_SetString(PyExc_TypeError, "retval should be None");
        Py_DECREF(py_retval);
        Py_DECREF(py_method);
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    
    Py_DECREF(py_retval);
    Py_DECREF(py_method);
    Py_DECREF(py_args);
    Py_DECREF(py_self);
    pyg_gil_state_release(__py_state);
}
#line 966 "glade-python-gwa.c"


#line 719 "glade-python-gwa.override"
static void
_wrap_GladeWidgetAdaptor__proxy_do_child_get_property(GladeWidgetAdaptor *self, GObject*container, GObject*child, const gchar*property_name, GValue *value)
{
    PyGILState_STATE __py_state;
    PyObject *py_self;
    PyObject *py_container = NULL;
    PyObject *py_child = NULL;
    PyObject *py_property_name;
    PyObject *py_retval;
    PyObject *py_args;
    PyObject *py_method;
    
    __py_state = pyg_gil_state_ensure();
    py_self = pygobject_new((GObject *) self);
    if (!py_self) {
        if (PyErr_Occurred())
            PyErr_Print();
        pyg_gil_state_release(__py_state);
        return;
    }
    if (container)
        py_container = pygobject_new((GObject *) container);
    else {
        Py_INCREF(Py_None);
        py_container = Py_None;
    }
    if (child)
        py_child = pygobject_new((GObject *) child);
    else {
        Py_INCREF(Py_None);
        py_child = Py_None;
    }
    py_property_name = PyString_FromString(property_name);
    if (!py_property_name) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_child);
        Py_DECREF(py_container);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    
    py_args = PyTuple_New(3);
    PyTuple_SET_ITEM(py_args, 0, py_container);
    PyTuple_SET_ITEM(py_args, 1, py_child);
    PyTuple_SET_ITEM(py_args, 2, py_property_name);
    
    py_method = PyObject_GetAttrString(py_self, "do_child_get_property");
    if (!py_method) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    py_retval = PyObject_CallObject(py_method, py_args);
    if (!py_retval) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_method);
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    
    pyg_value_from_pyobject (value, py_retval);
   
    Py_DECREF(py_retval);
    Py_DECREF(py_method);
    Py_DECREF(py_args);
    Py_DECREF(py_self);
    pyg_gil_state_release(__py_state);
}
#line 1046 "glade-python-gwa.c"


#line 824 "glade-python-gwa.override"
static GList*
_wrap_GladeWidgetAdaptor__proxy_do_get_children(GladeWidgetAdaptor *self, GObject*container)
{
    PyGILState_STATE __py_state;
    PyObject *py_self;
    PyObject *py_container = NULL;
    GList* retval;
    PyObject *py_retval;
    PyObject *py_args;
    PyObject *py_method;
    
    __py_state = pyg_gil_state_ensure();
    py_self = pygobject_new((GObject *) self);
    if (!py_self) {
        if (PyErr_Occurred())
            PyErr_Print();
        pyg_gil_state_release(__py_state);
        return NULL;
    }
    if (container)
        py_container = pygobject_new((GObject *) container);
    else {
        Py_INCREF(Py_None);
        py_container = Py_None;
    }
    
    py_args = PyTuple_New(1);
    PyTuple_SET_ITEM(py_args, 0, py_container);
    
    py_method = PyObject_GetAttrString(py_self, "do_get_children");
    if (!py_method) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return NULL;
    }
    py_retval = PyObject_CallObject(py_method, py_args);
    if (!py_retval) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_method);
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return NULL;
    }
	
    retval = glade_python_support_glist_from_list (py_retval);
    
    Py_DECREF(py_retval);
    Py_DECREF(py_method);
    Py_DECREF(py_args);
    Py_DECREF(py_self);
    pyg_gil_state_release(__py_state);
    
    return retval;
}
#line 1109 "glade-python-gwa.c"


static void
_wrap_GladeWidgetAdaptor__proxy_do_add(GladeWidgetAdaptor *self, GObject*parent, GObject*child)
{
    PyGILState_STATE __py_state;
    PyObject *py_self;
    PyObject *py_parent = NULL;
    PyObject *py_child = NULL;
    PyObject *py_retval;
    PyObject *py_args;
    PyObject *py_method;
    
    __py_state = pyg_gil_state_ensure();
    py_self = pygobject_new((GObject *) self);
    if (!py_self) {
        if (PyErr_Occurred())
            PyErr_Print();
        pyg_gil_state_release(__py_state);
        return;
    }
    if (parent)
        py_parent = pygobject_new((GObject *) parent);
    else {
        Py_INCREF(Py_None);
        py_parent = Py_None;
    }
    if (child)
        py_child = pygobject_new((GObject *) child);
    else {
        Py_INCREF(Py_None);
        py_child = Py_None;
    }
    
    py_args = PyTuple_New(2);
    PyTuple_SET_ITEM(py_args, 0, py_parent);
    PyTuple_SET_ITEM(py_args, 1, py_child);
    
    py_method = PyObject_GetAttrString(py_self, "do_add");
    if (!py_method) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    py_retval = PyObject_CallObject(py_method, py_args);
    if (!py_retval) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_method);
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    if (py_retval != Py_None) {
        if (PyErr_Occurred())
            PyErr_Print();
        PyErr_SetString(PyExc_TypeError, "retval should be None");
        Py_DECREF(py_retval);
        Py_DECREF(py_method);
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    
    Py_DECREF(py_retval);
    Py_DECREF(py_method);
    Py_DECREF(py_args);
    Py_DECREF(py_self);
    pyg_gil_state_release(__py_state);
}
static void
_wrap_GladeWidgetAdaptor__proxy_do_remove(GladeWidgetAdaptor *self, GObject*parent, GObject*child)
{
    PyGILState_STATE __py_state;
    PyObject *py_self;
    PyObject *py_parent = NULL;
    PyObject *py_child = NULL;
    PyObject *py_retval;
    PyObject *py_args;
    PyObject *py_method;
    
    __py_state = pyg_gil_state_ensure();
    py_self = pygobject_new((GObject *) self);
    if (!py_self) {
        if (PyErr_Occurred())
            PyErr_Print();
        pyg_gil_state_release(__py_state);
        return;
    }
    if (parent)
        py_parent = pygobject_new((GObject *) parent);
    else {
        Py_INCREF(Py_None);
        py_parent = Py_None;
    }
    if (child)
        py_child = pygobject_new((GObject *) child);
    else {
        Py_INCREF(Py_None);
        py_child = Py_None;
    }
    
    py_args = PyTuple_New(2);
    PyTuple_SET_ITEM(py_args, 0, py_parent);
    PyTuple_SET_ITEM(py_args, 1, py_child);
    
    py_method = PyObject_GetAttrString(py_self, "do_remove");
    if (!py_method) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    py_retval = PyObject_CallObject(py_method, py_args);
    if (!py_retval) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_method);
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    if (py_retval != Py_None) {
        if (PyErr_Occurred())
            PyErr_Print();
        PyErr_SetString(PyExc_TypeError, "retval should be None");
        Py_DECREF(py_retval);
        Py_DECREF(py_method);
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    
    Py_DECREF(py_retval);
    Py_DECREF(py_method);
    Py_DECREF(py_args);
    Py_DECREF(py_self);
    pyg_gil_state_release(__py_state);
}
static void
_wrap_GladeWidgetAdaptor__proxy_do_replace_child(GladeWidgetAdaptor *self, GObject*container, GObject*old, GObject*new)
{
    PyGILState_STATE __py_state;
    PyObject *py_self;
    PyObject *py_container = NULL;
    PyObject *py_old = NULL;
    PyObject *py_new = NULL;
    PyObject *py_retval;
    PyObject *py_args;
    PyObject *py_method;
    
    __py_state = pyg_gil_state_ensure();
    py_self = pygobject_new((GObject *) self);
    if (!py_self) {
        if (PyErr_Occurred())
            PyErr_Print();
        pyg_gil_state_release(__py_state);
        return;
    }
    if (container)
        py_container = pygobject_new((GObject *) container);
    else {
        Py_INCREF(Py_None);
        py_container = Py_None;
    }
    if (old)
        py_old = pygobject_new((GObject *) old);
    else {
        Py_INCREF(Py_None);
        py_old = Py_None;
    }
    if (new)
        py_new = pygobject_new((GObject *) new);
    else {
        Py_INCREF(Py_None);
        py_new = Py_None;
    }
    
    py_args = PyTuple_New(3);
    PyTuple_SET_ITEM(py_args, 0, py_container);
    PyTuple_SET_ITEM(py_args, 1, py_old);
    PyTuple_SET_ITEM(py_args, 2, py_new);
    
    py_method = PyObject_GetAttrString(py_self, "do_replace_child");
    if (!py_method) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    py_retval = PyObject_CallObject(py_method, py_args);
    if (!py_retval) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_method);
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    if (py_retval != Py_None) {
        if (PyErr_Occurred())
            PyErr_Print();
        PyErr_SetString(PyExc_TypeError, "retval should be None");
        Py_DECREF(py_retval);
        Py_DECREF(py_method);
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    
    Py_DECREF(py_retval);
    Py_DECREF(py_method);
    Py_DECREF(py_args);
    Py_DECREF(py_self);
    pyg_gil_state_release(__py_state);
}
static void
_wrap_GladeWidgetAdaptor__proxy_do_post_create(GladeWidgetAdaptor *self, GObject*object, GladeCreateReason reason)
{
    PyGILState_STATE __py_state;
    PyObject *py_self;
    PyObject *py_object = NULL;
    PyObject *py_reason;
    PyObject *py_retval;
    PyObject *py_args;
    PyObject *py_method;
    
    __py_state = pyg_gil_state_ensure();
    py_self = pygobject_new((GObject *) self);
    if (!py_self) {
        if (PyErr_Occurred())
            PyErr_Print();
        pyg_gil_state_release(__py_state);
        return;
    }
    if (object)
        py_object = pygobject_new((GObject *) object);
    else {
        Py_INCREF(Py_None);
        py_object = Py_None;
    }
    py_reason = pyg_enum_from_gtype(GLADE_CREATE_REASON, reason);
    if (!py_reason) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_object);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    
    py_args = PyTuple_New(2);
    PyTuple_SET_ITEM(py_args, 0, py_object);
    PyTuple_SET_ITEM(py_args, 1, py_reason);
    
    py_method = PyObject_GetAttrString(py_self, "do_post_create");
    if (!py_method) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    py_retval = PyObject_CallObject(py_method, py_args);
    if (!py_retval) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_method);
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    if (py_retval != Py_None) {
        if (PyErr_Occurred())
            PyErr_Print();
        PyErr_SetString(PyExc_TypeError, "retval should be None");
        Py_DECREF(py_retval);
        Py_DECREF(py_method);
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    
    Py_DECREF(py_retval);
    Py_DECREF(py_method);
    Py_DECREF(py_args);
    Py_DECREF(py_self);
    pyg_gil_state_release(__py_state);
}
static GObject*
_wrap_GladeWidgetAdaptor__proxy_do_get_internal_child(GladeWidgetAdaptor *self, GObject*parent, const gchar*name)
{
    PyGILState_STATE __py_state;
    PyObject *py_self;
    PyObject *py_parent = NULL;
    PyObject *py_name;
    GObject* retval;
    PyObject *py_retval;
    PyObject *py_args;
    PyObject *py_method;
    
    __py_state = pyg_gil_state_ensure();
    py_self = pygobject_new((GObject *) self);
    if (!py_self) {
        if (PyErr_Occurred())
            PyErr_Print();
        pyg_gil_state_release(__py_state);
        return NULL;
    }
    if (parent)
        py_parent = pygobject_new((GObject *) parent);
    else {
        Py_INCREF(Py_None);
        py_parent = Py_None;
    }
    py_name = PyString_FromString(name);
    if (!py_name) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_parent);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return NULL;
    }
    
    py_args = PyTuple_New(2);
    PyTuple_SET_ITEM(py_args, 0, py_parent);
    PyTuple_SET_ITEM(py_args, 1, py_name);
    
    py_method = PyObject_GetAttrString(py_self, "do_get_internal_child");
    if (!py_method) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return NULL;
    }
    py_retval = PyObject_CallObject(py_method, py_args);
    if (!py_retval) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_method);
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return NULL;
    }
    if (!PyObject_TypeCheck(py_retval, &PyGObject_Type)) {
        PyErr_SetString(PyExc_TypeError, "retval should be a GObject");
        PyErr_Print();
        Py_DECREF(py_retval);
        Py_DECREF(py_method);
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return NULL;
    }
    retval = (GObject*) pygobject_get(py_retval);
    g_object_ref((GObject *) retval);
    
    Py_DECREF(py_retval);
    Py_DECREF(py_method);
    Py_DECREF(py_args);
    Py_DECREF(py_self);
    pyg_gil_state_release(__py_state);
    
    return retval;
}
static void
_wrap_GladeWidgetAdaptor__proxy_do_launch_editor(GladeWidgetAdaptor *self, GObject*object)
{
    PyGILState_STATE __py_state;
    PyObject *py_self;
    PyObject *py_object = NULL;
    PyObject *py_retval;
    PyObject *py_args;
    PyObject *py_method;
    
    __py_state = pyg_gil_state_ensure();
    py_self = pygobject_new((GObject *) self);
    if (!py_self) {
        if (PyErr_Occurred())
            PyErr_Print();
        pyg_gil_state_release(__py_state);
        return;
    }
    if (object)
        py_object = pygobject_new((GObject *) object);
    else {
        Py_INCREF(Py_None);
        py_object = Py_None;
    }
    
    py_args = PyTuple_New(1);
    PyTuple_SET_ITEM(py_args, 0, py_object);
    
    py_method = PyObject_GetAttrString(py_self, "do_launch_editor");
    if (!py_method) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    py_retval = PyObject_CallObject(py_method, py_args);
    if (!py_retval) {
        if (PyErr_Occurred())
            PyErr_Print();
        Py_DECREF(py_method);
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    if (py_retval != Py_None) {
        if (PyErr_Occurred())
            PyErr_Print();
        PyErr_SetString(PyExc_TypeError, "retval should be None");
        Py_DECREF(py_retval);
        Py_DECREF(py_method);
        Py_DECREF(py_args);
        Py_DECREF(py_self);
        pyg_gil_state_release(__py_state);
        return;
    }
    
    Py_DECREF(py_retval);
    Py_DECREF(py_method);
    Py_DECREF(py_args);
    Py_DECREF(py_self);
    pyg_gil_state_release(__py_state);
}

static int
__GladeWidgetAdaptor_class_init(gpointer gclass, PyTypeObject *pyclass)
{
    PyObject *o;
    GladeWidgetAdaptorClass *klass = GLADE_WIDGET_ADAPTOR_CLASS(gclass);
    PyObject *gsignals = PyDict_GetItemString(pyclass->tp_dict, "__gsignals__");

    if ((o = PyDict_GetItemString(pyclass->tp_dict, "do_verify_property"))
        && !PyObject_TypeCheck(o, &PyCFunction_Type)
        && !(gsignals && PyDict_GetItemString(gsignals, "verify_property")))
        klass->verify_property = _wrap_GladeWidgetAdaptor__proxy_do_verify_property;

    if ((o = PyDict_GetItemString(pyclass->tp_dict, "do_set_property"))
        && !PyObject_TypeCheck(o, &PyCFunction_Type)
        && !(gsignals && PyDict_GetItemString(gsignals, "set_property")))
        klass->set_property = _wrap_GladeWidgetAdaptor__proxy_do_set_property;

    if ((o = PyDict_GetItemString(pyclass->tp_dict, "do_get_property"))
        && !PyObject_TypeCheck(o, &PyCFunction_Type)
        && !(gsignals && PyDict_GetItemString(gsignals, "get_property")))
        klass->get_property = _wrap_GladeWidgetAdaptor__proxy_do_get_property;

    if ((o = PyDict_GetItemString(pyclass->tp_dict, "do_child_verify_property"))
        && !PyObject_TypeCheck(o, &PyCFunction_Type)
        && !(gsignals && PyDict_GetItemString(gsignals, "child_verify_property")))
        klass->child_verify_property = _wrap_GladeWidgetAdaptor__proxy_do_child_verify_property;

    if ((o = PyDict_GetItemString(pyclass->tp_dict, "do_child_set_property"))
        && !PyObject_TypeCheck(o, &PyCFunction_Type)
        && !(gsignals && PyDict_GetItemString(gsignals, "child_set_property")))
        klass->child_set_property = _wrap_GladeWidgetAdaptor__proxy_do_child_set_property;

    if ((o = PyDict_GetItemString(pyclass->tp_dict, "do_child_get_property"))
        && !PyObject_TypeCheck(o, &PyCFunction_Type)
        && !(gsignals && PyDict_GetItemString(gsignals, "child_get_property")))
        klass->child_get_property = _wrap_GladeWidgetAdaptor__proxy_do_child_get_property;

    if ((o = PyDict_GetItemString(pyclass->tp_dict, "do_get_children"))
        && !PyObject_TypeCheck(o, &PyCFunction_Type)
        && !(gsignals && PyDict_GetItemString(gsignals, "get_children")))
        klass->get_children = _wrap_GladeWidgetAdaptor__proxy_do_get_children;

    if ((o = PyDict_GetItemString(pyclass->tp_dict, "do_add"))
        && !PyObject_TypeCheck(o, &PyCFunction_Type)
        && !(gsignals && PyDict_GetItemString(gsignals, "add")))
        klass->add = _wrap_GladeWidgetAdaptor__proxy_do_add;

    if ((o = PyDict_GetItemString(pyclass->tp_dict, "do_remove"))
        && !PyObject_TypeCheck(o, &PyCFunction_Type)
        && !(gsignals && PyDict_GetItemString(gsignals, "remove")))
        klass->remove = _wrap_GladeWidgetAdaptor__proxy_do_remove;

    if ((o = PyDict_GetItemString(pyclass->tp_dict, "do_replace_child"))
        && !PyObject_TypeCheck(o, &PyCFunction_Type)
        && !(gsignals && PyDict_GetItemString(gsignals, "replace_child")))
        klass->replace_child = _wrap_GladeWidgetAdaptor__proxy_do_replace_child;

    if ((o = PyDict_GetItemString(pyclass->tp_dict, "do_post_create"))
        && !PyObject_TypeCheck(o, &PyCFunction_Type)
        && !(gsignals && PyDict_GetItemString(gsignals, "post_create")))
        klass->post_create = _wrap_GladeWidgetAdaptor__proxy_do_post_create;

    if ((o = PyDict_GetItemString(pyclass->tp_dict, "do_get_internal_child"))
        && !PyObject_TypeCheck(o, &PyCFunction_Type)
        && !(gsignals && PyDict_GetItemString(gsignals, "get_internal_child")))
        klass->get_internal_child = _wrap_GladeWidgetAdaptor__proxy_do_get_internal_child;

    if ((o = PyDict_GetItemString(pyclass->tp_dict, "do_launch_editor"))
        && !PyObject_TypeCheck(o, &PyCFunction_Type)
        && !(gsignals && PyDict_GetItemString(gsignals, "launch_editor")))
        klass->launch_editor = _wrap_GladeWidgetAdaptor__proxy_do_launch_editor;
    return 0;
}


/* ----------- functions ----------- */

PyMethodDef glade_python_gwa_functions[] = {
    { NULL, NULL, 0 }
};


/* ----------- enums and flags ----------- */

void
glade_python_gwa_add_constants(PyObject *module, const gchar *strip_prefix)
{
  pyg_enum_add(module, "GladeCreateReason", strip_prefix, GLADE_CREATE_REASON);

  if (PyErr_Occurred())
    PyErr_Print();
}

/* initialise stuff extension classes */
void
glade_python_gwa_register_classes(PyObject *d)
{
    PyObject *module;

    if ((module = PyImport_ImportModule("gobject")) != NULL) {
        PyObject *moddict = PyModule_GetDict(module);

        _PyGObject_Type = (PyTypeObject *)PyDict_GetItemString(moddict, "GObject");
        if (_PyGObject_Type == NULL) {
            PyErr_SetString(PyExc_ImportError,
                "cannot import name GObject from gobject");
            return;
        }
    } else {
        PyErr_SetString(PyExc_ImportError,
            "could not import gobject");
        return;
    }


#line 1676 "glade-python-gwa.c"
    pygobject_register_class(d, "GladeWidgetAdaptor", GLADE_TYPE_WIDGET_ADAPTOR, &PyGladeWidgetAdaptor_Type, Py_BuildValue("(O)", &PyGObject_Type));
    pyg_set_object_has_new_constructor(GLADE_TYPE_WIDGET_ADAPTOR);
    pyg_register_class_init(GLADE_TYPE_WIDGET_ADAPTOR, __GladeWidgetAdaptor_class_init);
}
