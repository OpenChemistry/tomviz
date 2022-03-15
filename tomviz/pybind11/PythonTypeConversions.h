/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPythonTypeConversions_h
#define tomvizPythonTypeConversions_h

#include "core/Variant.h"

// Forward declare PyObject
// See https://mail.python.org/pipermail/python-dev/2003-August/037601.html
#ifndef PyObject_HEAD
struct _object;
typedef _object PyObject;
#endif

namespace tomviz {

PyObject* toPyObject(int i);
PyObject* toPyObject(long l);
PyObject* toPyObject(double d);
PyObject* toPyObject(bool b);
PyObject* toPyObject(const std::string& str);
PyObject* toPyObject(const Variant& value);
PyObject* toPyObject(const std::vector<Variant>& list);
PyObject* toPyObject(const std::map<std::string, Variant>& map);

} // namespace tomviz

#endif
