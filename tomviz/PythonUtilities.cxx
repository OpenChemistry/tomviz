/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PythonUtilities.h"

#include "vtkPython.h" // must be first

#include "DataSource.h"
#include "Logger.h"
#include "OperatorFactory.h"
#include <QDebug>

#include <vtkPythonInterpreter.h>
#include <vtkPythonUtil.h>
#include <vtkSmartPyObject.h>

#include <pybind11/pybind11.h>

namespace py = pybind11;

namespace tomviz {

Python::Capsule::Capsule(const void* ptr)
  : m_capsule(new pybind11::capsule(ptr))
{}

Python::Capsule::operator PyObject*() const
{
  return m_capsule->ptr();
}

void Python::Capsule::incrementRefCount()
{
  m_capsule->inc_ref();
}

Python::Capsule::~Capsule()
{
  delete m_capsule;
}

Python::Object::Object() : m_smartPyObject(new vtkSmartPyObject()) {}

Python::Object::Object(const Python::Object& other)
  : m_smartPyObject(new vtkSmartPyObject(*other.m_smartPyObject))
{}

Python::Object::Object(const QString& str)
{
  m_smartPyObject = new vtkSmartPyObject(toPyObject(str));
}

Python::Object::Object(const Variant& value)
{
  m_smartPyObject = new vtkSmartPyObject(toPyObject(value));
}

Python::Object::Object(const DataSource& source)
{
  // The vtkSmartPyObject will take ownership of the PyObject*
  py::object obj = py::cast(source, py::return_value_policy::reference);
  m_smartPyObject = new vtkSmartPyObject(obj.release().ptr());
}

Python::Object::Object(PyObject* obj)
{
  m_smartPyObject = new vtkSmartPyObject(obj);
}

Python::Object& Python::Object::operator=(const Python::Object& other)
{
  *m_smartPyObject = *other.m_smartPyObject;
  return *this;
}

Python::Object::operator PyObject*() const
{
  return *m_smartPyObject;
}

Python::Object::operator bool() const
{
  return *m_smartPyObject;
}

bool Python::Object::toBool() const
{
  return m_smartPyObject->GetPointer() == Py_True;
}

bool Python::Object::isDict() const
{
  return PyDict_Check(m_smartPyObject->GetPointer());
}

bool Python::Object::isList() const
{
  return PyList_Check(m_smartPyObject->GetPointer());
}

bool Python::Object::isTuple() const
{
  return PyTuple_Check(m_smartPyObject->GetPointer());
}

bool Python::Object::isValid() const
{
  return m_smartPyObject->GetPointer() != nullptr;
}

Python::Dict Python::Object::toDict()
{
  return m_smartPyObject->GetPointer();
}

Python::List Python::Object::toList()
{
  return m_smartPyObject->GetPointer();
}

QString Python::Object::toString() const
{
// Function documentation says the caller of either of the functions below
// is not responsible for deallocating the buffer.
#if PY_MAJOR_VERSION >= 3
  const char* cdata = PyUnicode_AsUTF8(m_smartPyObject->GetPointer());
#else
  const char* cdata = PyBytes_AsString(m_smartPyObject->GetPointer());
#endif

  return QString(cdata ? cdata : "");
}

Python::Object Python::Object::getAttr(const QString& name)
{
  Object attrName(name);
  auto& attrNamePyObj = *attrName;
  return PyObject_GetAttr(m_smartPyObject->GetPointer(), &attrNamePyObj);
}

Python::Object::~Object()
{
  delete m_smartPyObject;
}

void Python::Object::incrementRefCount()
{
  m_smartPyObject->GetAndIncreaseReferenceCount();
}

Python::Tuple::Tuple() : Object() {}

Python::Tuple::Tuple(const Python::Tuple& other) : Object(other) {}

Python::Tuple::Tuple(int size)
{
  m_smartPyObject->TakeReference(PyTuple_New(size));
}

void Python::Tuple::set(int index, Python::Object& obj)
{
  // Note we increment ref count
  // as PyTuple_SET_ITEM will "steal" the reference.
  obj.incrementRefCount();
  PyTuple_SET_ITEM(m_smartPyObject->GetPointer(), index, obj);
}

void Python::Tuple::set(int index, Python::Module& module)
{
  // Note we increment ref count
  // as PyTuple_SET_ITEM will "steal" the reference.
  module.incrementRefCount();
  PyTuple_SET_ITEM(m_smartPyObject->GetPointer(), index, module);
}

void Python::Tuple::set(int index, Python::Capsule& capsule)
{
  // Increment ref count as the pybind11::capsule destructor will decrement
  // and we need the capsule to stay around. The PyTuple_SET_ITEM will
  // steal the other reference.
  capsule.incrementRefCount();
  PyTuple_SET_ITEM(m_smartPyObject->GetPointer(), index, capsule);
}

void Python::Tuple::set(int index, const Variant& value)
{
  Python::Object pyObj(toPyObject(value));
  set(index, pyObj);
}

int Python::Tuple::length()
{
  return PyTuple_Size(m_smartPyObject->GetPointer());
}

Python::Object Python::Tuple::operator[](int index)
{
  PyObject* item = PyTuple_GetItem(m_smartPyObject->GetPointer(), index);
  // Increment ref count as our destructor will decrement it.
  Py_XINCREF(item);

  return PyTuple_GetItem(m_smartPyObject->GetPointer(), index);
}

Python::Dict::Dict() : Object()
{
  m_smartPyObject->TakeReference(PyDict_New());
}

Python::Dict::Dict(PyObject* obj) : Object(obj) {}

Python::Dict::Dict(const Python::Dict& other) : Object(other) {}

Python::Dict::Dict(const Object& obj) : Object(obj) {}

Python::Dict& Python::Dict::operator=(const Python::Object& other)
{
  Object::operator=(other);

  return *this;
}

Python::Object Python::Dict::operator[](const QString& key)
{
  return operator[](key.toLatin1().data());
}

Python::Object Python::Dict::operator[](const char* key)
{
  PyObject* item = PyDict_GetItemString(m_smartPyObject->GetPointer(), key);
  // Increment ref count as our destructor will decrement it.
  Py_XINCREF(item);

  return item;
}

void Python::Dict::set(const QString& key, const Object& value)
{
  Python::Object pyKey(key);
  PyDict_SetItem(m_smartPyObject->GetPointer(), pyKey, value);
}

void Python::Dict::set(const QString& key, const Variant& value)
{
  Python::Object obj(value);
  set(key, obj);
}

QString Python::Dict::toString()
{
  PyObject* objectRepr = PyObject_Repr(*this);
  return PyString_AsString(objectRepr);
}

Python::List::List(PyObject* obj) : Object(obj) {}

Python::List::List(const List& other) : Object(other) {}

Python::Object Python::List::operator[](int index)
{
  PyObject* item = PyList_GetItem(m_smartPyObject->GetPointer(), index);
  // Increment ref count as our destructor will decrement it.
  Py_XINCREF(item);

  return PyList_GetItem(m_smartPyObject->GetPointer(), index);
}

int Python::List::length()
{
  return PyList_Size(m_smartPyObject->GetPointer());
}

Python::Function::Function() : Object() {}

Python::Function::Function(PyObject* obj) : Object(obj) {}

Python::Function::Function(const Python::Function& other) : Object(other) {}

Python::Function& Python::Function::operator=(const Python::Object& other)
{
  Object::operator=(other);

  return *this;
}

Python::Object Python::Function::call()
{
  Python::Tuple empty(0);
  return call(empty);
}

Python::Object Python::Function::call(Tuple& args)
{
  Python::Dict empty;
  return call(args, empty);
}

Python::Object Python::Function::call(Tuple& args, Dict& kwargs)
{
  Python::Object result =
    PyObject_Call(m_smartPyObject->GetPointer(), args, kwargs);

  if (!result.isValid()) {
    checkForPythonError();
  }

  return result;
}

QString Python::Function::toString()
{
  PyObject* objectRepr = PyObject_Repr(*this);
  return PyString_AsString(objectRepr);
}

Python::Object Python::VTK::GetObjectFromPointer(vtkObjectBase* ptr)
{
  return vtkPythonUtil::GetObjectFromPointer(ptr);
}

vtkObjectBase* Python::VTK::GetPointerFromObject(Python::Object obj,
                                                 const char* classname)
{
  return vtkPythonUtil::GetPointerFromObject(obj, classname);
}

vtkObjectBase* Python::VTK::convertToDataObject(Python::Object obj)
{
  Python python;

  auto internalModule = python.import("tomviz._internal");
  if (!internalModule.isValid()) {
    Logger::critical("Failed to import tomviz._internal module.");
  }

  auto convertFunc = internalModule.findFunction("convert_to_vtk_data_object");
  if (!convertFunc.isValid()) {
    Logger::critical("Unable to locate convert_to_vtk_data_object.");
  }

  Python::Tuple args(1);
  args.set(0, obj);

  auto dataObject = convertFunc.call(args);
  if (!dataObject.isValid()) {
    Logger::critical("Failed to execute convert_to_vtk_data_object.");
  }

  return GetPointerFromObject(dataObject, "vtkDataObject");
}

void Python::initialize()
{
  vtkPythonInterpreter::Initialize();
}

Python::Python() : m_ensurer(new vtkPythonScopeGilEnsurer(true)) {}

Python::~Python()
{
  delete m_ensurer;
}

Python::Module Python::import(const QString& name)
{
  Python::Module module = PyImport_ImportModule(name.toLatin1().data());
  if (!module.isValid()) {
    checkForPythonError();
  }

  return module;
}

Python::Module::Module() : Object() {}

Python::Module::Module(PyObject* obj) : Object(obj) {}

Python::Module::Module(const Python::Module& other) : Object(other) {}

Python::Module& Python::Module::operator=(const Python::Module& other)
{
  Python::Object::operator=(other);
  return *this;
}

Python::Function Python::Module::findFunction(const QString& name)
{

  Python::Function func = PyObject_GetAttrString(m_smartPyObject->GetPointer(),
                                                 name.toLatin1().data());
  if (!func.isValid()) {
    checkForPythonError();
  }

  return func;
}

Python::Module Python::import(const QString& str, const QString& filename,
                              const QString& moduleName)
{

  Python::Module module;

  vtkSmartPyObject code =
    Py_CompileString(str.toLatin1().data(), filename.toLatin1().data(),
                     Py_file_input /*Py_eval_input*/);
  if (!code) {
    checkForPythonError();
    Logger::critical(
      "Invalid script. Please check the traceback message for details");
    return module;
  }

  module = PyImport_ExecCodeModule(moduleName.toLatin1().data(), code);
  if (!module.isValid()) {
    checkForPythonError();
    Logger::critical("Failed to create module.");
    return module;
  }

  return module;
}

bool Python::checkForPythonError()
{
  PyObject* exception = PyErr_Occurred();
  if (exception) {
    // We use PyErr_PrintEx(0) to prevent sys.last_traceback being set
    // which holds a reference to any parameters passed to PyObject_Call.
    // This can cause a temporary "leak" until sys.last_traceback is reset.
    // This can be a problem if the object in question is a VTK object that
    // holds a reference to a large memory allocation.
    PyErr_PrintEx(0);
    return true;
  }
  return false;
}

PyObject* Python::toPyObject(const QString& str)
{
  return PyUnicode_DecodeUTF16((const char*)str.utf16(), str.length() * 2, NULL,
                               NULL);
}

PyObject* Python::toPyObject(const std::string& str)
{
  return PyString_FromStringAndSize(str.data(), str.size());
}

PyObject* Python::toPyObject(const Variant& value)
{

  switch (value.type()) {
    case Variant::INTEGER:
      return toPyObject(value.toInteger());
    case Variant::DOUBLE:
      return PyFloat_FromDouble(value.toDouble());
    case Variant::BOOL:
      return value.toBool() ? Py_True : Py_False;
    case Variant::STRING: {
      return toPyObject(value.toString());
    }
    case Variant::LIST: {
      std::vector<Variant> list = value.toList();
      return toPyObject(list);
    }
    default:
      Logger::critical("Unsupported type");
  }

  return nullptr;
}

PyObject* Python::toPyObject(const std::vector<Variant>& list)
{
  PyObject* pyList = PyTuple_New(list.size());
  int i = 0;

  foreach (Variant value, list) {
    PyTuple_SET_ITEM(pyList, i, toPyObject(value));
    i++;
  }

  return pyList;
}

PyObject* Python::toPyObject(long l)
{
  return PyInt_FromLong(l);
}

void Python::prependPythonPath(std::string dir)
{
  vtkPythonInterpreter::PrependPythonPath(dir.c_str());
}

Python::Object Python::createDataset(vtkObjectBase* data,
                                     const DataSource& source)
{
  Python python;
  auto module = python.import("tomviz.internal_dataset");
  if (!module.isValid()) {
    Logger::critical("Failed to import tomviz.internal_dataset module.");
  }

  auto createDatasetFunc = module.findFunction("create_dataset");
  if (!createDatasetFunc.isValid()) {
    Logger::critical("Unable to locate create_dataset.");
  }

  auto dataObj = Python::VTK::GetObjectFromPointer(data);
  auto dataSourceObj = Python::Object(source);

  Python::Tuple args(2);
  args.set(0, dataObj);
  args.set(1, dataSourceObj);

  return createDatasetFunc.call(args);
}

std::vector<OperatorDescription> findCustomOperators(const QString& path)
{
  Python python;
  auto internalModule = python.import("tomviz._internal");
  if (!internalModule.isValid()) {
    Logger::critical("Failed to import tomviz._internal module.");
  }

  auto findCustomOperators = internalModule.findFunction("find_operators");
  if (!findCustomOperators.isValid()) {
    Logger::critical("Unable to locate find_operators.");
  }

  std::vector<OperatorDescription> operators;
  Python::Object pyPath(path);
  Python::Tuple args(1);
  args.set(0, pyPath);

  auto pyOperators = findCustomOperators.call(args);
  if (!pyOperators.isValid()) {
    Logger::critical("Failed to execute findCustomOperators.");
  }

  Python::List ops(pyOperators);
  for (int i = 0; i < ops.length(); i++) {
    Python::Dict opDict = ops[i].toDict();
    OperatorDescription op;

    op.label = opDict["label"].toString();
    op.pythonPath = opDict["pythonPath"].toString();
    op.valid = opDict["valid"].toBool();

    // Do we have a JSON file?
    Python::Object jsonPath = opDict["jsonPath"];
    if (jsonPath.isValid()) {
      op.jsonPath = jsonPath.toString();
    }

    // Do we have a load error?
    Python::Object loadError = opDict["loadError"];
    if (loadError.isValid()) {
      op.loadError = loadError.toString();
    }

    operators.push_back(op);
  }

  return operators;
}

TemporarilyReleaseGil::TemporarilyReleaseGil()
{
  // This releases the current thread's lock and saves thread state. We need
  // to do this so the main thread can acquire the lock for certain operations.
  m_save = PyEval_SaveThread();
}

TemporarilyReleaseGil::~TemporarilyReleaseGil()
{
  // Have this thread reacquire the lock and restore the previous thread state.
  PyEval_RestoreThread(m_save);
}
} // namespace tomviz
