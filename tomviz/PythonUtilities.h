/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#ifndef tomvizPythonUtilties_h
#define tomvizPythonUtilties_h

// Collection of miscellaneous Python utility functions.

#include "Variant.h"
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

  class Object
  {
  public:
    Object();
    Object(const Object& other);
    Object(const QString& str);
    Object(const Variant& value);
    Object(PyObject* obj);

    Object& operator=(const Object& other);
    operator PyObject*() const;
    operator bool() const;

    void incrementRefCount();
    bool toBool() const;
    bool isDict() const;
    bool isValid() const;
    QString toString() const;
    Dict toDict();
    virtual ~Object();

  protected:
    vtkSmartPyObject* m_smartPyObject = nullptr;
  };

  class Module;

  class Tuple : public Object
  {
  public:
    Tuple();
    Tuple(const Tuple& other);
    Tuple(int size);
    void set(int index, Module& obj);
    void set(int index, Capsule& obj);
    void set(int index, Object& obj);
    void set(int index, const Variant& value);
  };

  class Dict : public Object
  {
  public:
    Dict();
    Dict(PyObject* obj);
    Dict(const Dict& other);
    Dict(const Object& obj);
    Dict& operator=(const Object& other);
    Object operator[](const QString& key);
    Object operator[](const char* key);
    void set(const QString& key, const Object& value);
    void set(const QString& key, const Variant& value);
    QString toString();
  };

  class List : public Object
  {
  public:
    List(PyObject* obj);
    List(const List& other);
    Object operator[](int index);
    int length();
  };

  class Function : public Object
  {
  public:
    Function();
    Function(PyObject* obj);
    Function(const Function& other);
    Function& operator=(const Object& other);

    Object call(Tuple& args);
    Object call(Tuple& args, Dict& kwargs);
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

  /// Convert a tomviz::Variant to the appropriate Python type
  static PyObject* toPyObject(const Variant& variant);

  /// Convert a QString to the appropriate Python type
  static PyObject* toPyObject(const QString& str);

  /// Convert a std::string to the appropriate Python type
  static PyObject* toPyObject(const std::string& str);

  /// Convert a list of tomviz::Variant to the appropriate Python types
  static PyObject* toPyObject(const std::vector<Variant>& variants);

  /// Convert a long to the appropriate Python type
  static PyObject* toPyObject(long l);

  /// Prepends the path to the sys.path variable calls
  /// vtkPythonPythonInterpreter::PrependPythonPath(...)  to do the work.
  static void prependPythonPath(std::string dir);

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
}

#endif
