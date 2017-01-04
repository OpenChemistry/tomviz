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

#include <QDebug>
#include <QList>
#include <QString>
#include <QVariant>


// Forward declare PyObject
// See https://mail.python.org/pipermail/python-dev/2003-August/037601.html
#ifndef PyObject_HEAD
struct _object;
typedef _object PyObject;
#endif

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
    Object(const QVariant& value);
    Object(const QVariantList& list);
    Object(PyObject *obj);
    Object& operator=(const Object& other);
    operator PyObject*() const;
    operator bool() const;
    void incrementRefCount();
    bool toBool() const;
    bool isDict() const;
    bool isValid() const;
    Dict toDict();
    virtual ~Object();

  protected:
    vtkSmartPyObject* m_smartPyObject;
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
  };

  class Dict : public Object
  {
  public:
    Dict();
    Dict(PyObject *obj);
    Dict(const Dict& other);
    Object operator[](const QString& key);
    void set(const QString& key, const Object& value);
    void set(const QString& key, const QVariant& value);
    void set(const QString& key, const QString& str);
    void set(const QString& key, const QVariantList& list);
  };

  class Function : public Object
  {
  public:
    Function();
    Function(PyObject *obj);
    Function(const Function& other);
    Function& operator=(const Object& other);

    Object call(Tuple& args);
    Object call(Tuple& args, Dict& kwargs);
  };

  class Module : public Object
  {
  public:
    Module();
    Module(PyObject *obj);
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
  Module import(const QString& str, const QString& filename);

  /// Check for Python error. Prints error and clears it if an error has
  /// occurred.
  /// Return true if an error has occurred, false otherwise.
  static bool checkForPythonError();

  /// Convert a QString to a Python string
  static PyObject* toPyObject(const QString& str);

  /// Convert a QVariant object into the appropriate Python type
  static PyObject* toPyObject(const QVariant& value);

  // Convert a QVariantList into a Python list
  static PyObject* toPyObject(const QVariantList& list);

private:
  vtkPythonScopeGilEnsurer* m_ensurer = nullptr;
};

QDebug operator<<(QDebug dbg, const Python::Dict& dict);

}

#endif
