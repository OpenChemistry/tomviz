/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PythonTypeConversions.h"

#include "Python.h"

namespace tomviz {

PyObject* toPyObject(int i)
{
  return toPyObject(static_cast<long>(i));
}

PyObject* toPyObject(long l)
{
  return PyLong_FromLong(l);
}

PyObject* toPyObject(double x)
{
  return PyFloat_FromDouble(x);
}

PyObject* toPyObject(bool b)
{
  return b ? Py_True : Py_False;
}

PyObject* toPyObject(const std::string& str)
{
  return PyUnicode_FromStringAndSize(str.data(), str.size());
}

PyObject* toPyObject(const Variant& value)
{

  switch (value.type()) {
    case Variant::INTEGER:
      return toPyObject(value.toInteger());
    case Variant::LONG:
      return toPyObject(value.toLong());
    case Variant::DOUBLE:
      return toPyObject(value.toDouble());
    case Variant::BOOL:
      return toPyObject(value.toBool());
    case Variant::STRING:
      return toPyObject(value.toString());
    case Variant::LIST:
      return toPyObject(value.toList());
    case Variant::MAP:
      return toPyObject(value.toMap());
    default:
      std::cerr << "Unsupported type encountered: " << value.type() << "\n";
  }

  return nullptr;
}

PyObject* toPyObject(const std::vector<Variant>& list)
{
  PyObject* pyList = PyTuple_New(list.size());

  for (size_t i = 0; i < list.size(); ++i) {
    // PyTuple_SET_ITEM steals the reference
    PyTuple_SET_ITEM(pyList, i, toPyObject(list[i]));
  }

  return pyList;
}

PyObject* toPyObject(const std::map<std::string, Variant>& map)
{
  PyObject* dict = PyDict_New();
  for (const auto& x : map) {
    auto* value = toPyObject(x.second);
    PyDict_SetItemString(dict, x.first.c_str(), value);

    // PyDict_SetItem does not steal the reference, but increments it.
    // Therefore we should reduce the reference count here.
    Py_XDECREF(value);
  }

  return dict;
}

} // namespace tomviz
