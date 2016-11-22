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
#include "PythonUtilities.h"

#include <QDebug>

namespace tomviz {

bool checkForPythonError()
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

PyObject* toPyObject(const QString& str)
{
  return PyUnicode_DecodeUTF16((const char*)str.utf16(), str.length() * 2, NULL,
                               NULL);
}

PyObject* toPyObject(const QVariant& value)
{

  switch (value.type()) {
    case QVariant::Int:
      return PyInt_FromLong(value.toInt());
    case QVariant::Double:
      return PyFloat_FromDouble(value.toDouble());
    case QVariant::String: {
      QString str = value.toString();
      return toPyObject(str);
    }
    case QVariant::List: {
      QVariantList list = value.toList();
      return toPyObject(list);
    }
    default:
      qCritical() << "Unsupported type";
  }

  return nullptr;
}

PyObject* toPyObject(const QVariantList& list)
{
  PyObject* pyList = PyTuple_New(list.count());
  int i = 0;

  foreach (QVariant value, list) {
    PyTuple_SET_ITEM(pyList, i, toPyObject(value));
    i++;
  }

  return pyList;
}
}
