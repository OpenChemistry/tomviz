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
#include <pybind11/pybind11.h>

#include "OperatorPython.h"

using namespace tomviz;

struct OperatorPythonWrapper
{
  OperatorPythonWrapper(OperatorPython* o) { this->op = o; };
  bool canceled() { return this->op->isCanceled(); }
  void setTotalProgressSteps(int progress)
  {
    this->op->setTotalProgressSteps(progress);
  }
  int totalProgressSteps() { return this->op->totalProgressSteps(); }
  void updateProgress(int progress) { this->op->updateProgress(progress); }
  OperatorPython* op;
};

namespace py = pybind11;

PYBIND11_PLUGIN(_wrapping)
{
  py::module m("_wrapping", "tomviz wrapped classes");

  py::class_<OperatorPythonWrapper>(m, "OperatorPythonWrapper")
    .def("__init__",
         [](OperatorPythonWrapper& instance, void* op) {
           new (&instance)
             OperatorPythonWrapper(static_cast<OperatorPython*>(op));
         })
    .def_property_readonly("canceled", &OperatorPythonWrapper::canceled)
    .def_property("max_progress", &OperatorPythonWrapper::totalProgressSteps,
                  &OperatorPythonWrapper::setTotalProgressSteps)
    .def("update_progress", &OperatorPythonWrapper::updateProgress);

  return m.ptr();
}
