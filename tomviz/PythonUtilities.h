/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPythonUtilties_h
#define tomvizPythonUtilties_h

// Collection of miscellaneous Python utility functions.

#include "core/Variant.h"
#include <QString>

// Forward declare PyObject
// See https://mail.python.org/pipermail/python-dev/2003-August/037601.html
#ifndef PyObject_HEAD
struct _object;
typedef _object PyObject;
#endif

// Definition in pystate.h
typedef struct _ts PyThreadState;

class vtkSmartPyObject;
class vtkObjectBase;
class vtkPythonScopeGilEnsurer;

namespace pybind11 {
class capsule;
}

namespace tomviz {

class DataSource;
class DataSourceBase;

class Python
{

public:
  class Capsule
  {
  public:
    Capsule(const void* ptr);
    operator PyObject*() const;
    void incrementRefCount();
    ~Capsule();

  private:
    pybind11::capsule* m_capsule = nullptr;
  };

  class Dict;
  class List;
  class Object
  {
  public:
    Object();
    Object(const Object& other);
    Object(const QString& str);
    Object(const QStringList& str);
    Object(const QList<long>& intList);
    Object(const QList<double>& floatList);
    Object(const Variant& value);
    Object(const DataSourceBase& source);
    Object(PyObject* obj);

    Object& operator=(const Object& other);
    operator PyObject*() const;
    operator bool() const;

    void incrementRefCount();
    bool toBool() const;
    bool isBool() const;
    bool isString() const;
    bool isInt() const;
    bool isFloat() const;
    bool isDict() const;
    bool isList() const;
    bool isTuple() const;
    bool isValid() const;
    QString toString() const;
    double toDouble() const;
    long toLong() const;
    Dict toDict();
    List toList();
    virtual Variant toVariant();

    Object getAttr(const QString& name);

    virtual ~Object();

  protected:
    vtkSmartPyObject* m_smartPyObject = nullptr;
  };

  class Module;

  class Tuple : public Object
  {
  public:
    Tuple();
    Tuple(const Object& other);
    Tuple(const Tuple& other);
    Tuple(int size);
    void set(int index, Module& obj);
    void set(int index, Capsule& obj);
    void set(int index, Object& obj);
    void set(int index, const Variant& value);
    Object operator[](int index);
    int length();
    Variant toVariant() override;
  };

  class Dict : public Object
  {
  public:
    Dict();
    Dict(PyObject* obj);
    Dict(const Dict& other);
    Dict(const Object& obj);
    Dict(const std::map<std::string, Variant>& map);
    Dict& operator=(const Object& other);
    Object operator[](const QString& key);
    Object operator[](const std::string& key);
    Object operator[](const char* key);
    Object operator[](const Object& obj);
    void set(const QString& key, const Object& value);
    void set(const QString& key, const Variant& value);
    QString toString();
    Variant toVariant() override;
  };

  class List : public Object
  {
  public:
    List(PyObject* obj);
    List(const List& other);
    Object operator[](int index);
    int length();
    Variant toVariant() override;
  };

  class Function : public Object
  {
  public:
    Function();
    Function(PyObject* obj);
    Function(const Function& other);
    Function& operator=(const Function& other);
    Function& operator=(const Object& other);

    Object call();
    Object call(Tuple& args);
    Object call(Dict& kwargs);
    Object call(Tuple& args, Dict& kwargs);
    QString toString();
  };

  class Module : public Object
  {
  public:
    Module();
    Module(PyObject* obj);
    Module(const Module& other);
    Module& operator=(const Module& other);
    Function findFunction(const QString& name);
  };

  class VTK
  {
  public:
    static Object GetObjectFromPointer(vtkObjectBase* ptr);
    static vtkObjectBase* GetPointerFromObject(Object obj,
                                               const char* classname);
    // Performs conversions necessary to get a vtkDataObject, and
    // then returns the pointer.
    static vtkObjectBase* convertToDataObject(Object obj);
  };

  static void initialize();
  Python();
  ~Python();
  Module import(const QString& name);
  Module import(const QString& str, const QString& filename,
                const QString& moduleName);

  /// Check for Python error. Prints error and clears it if an error has
  /// occurred.
  /// Return true if an error has occurred, false otherwise.
  static bool checkForPythonError();

  /// Prepends the path to the sys.path variable calls
  /// vtkPythonPythonInterpreter::PrependPythonPath(...)  to do the work.
  static void prependPythonPath(std::string dir);

  /// Create an internal Dataset object for operators to use
  static Object createDataset(vtkObjectBase* data, const DataSource& source);

private:
  vtkPythonScopeGilEnsurer* m_ensurer = nullptr;
};

class TemporarilyReleaseGil
{

  PyThreadState* m_save = nullptr;

public:
  TemporarilyReleaseGil();
  ~TemporarilyReleaseGil();
};

struct OperatorDescription
{
  QString label;
  QString pythonPath;
  QString jsonPath;
  QString loadError;
  bool valid = true;
};

std::vector<OperatorDescription> findCustomOperators(const QString& path);
} // namespace tomviz

#endif
