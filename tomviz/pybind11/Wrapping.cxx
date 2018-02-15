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

#include "OperatorPythonWrapper.h"
#include "PybindVTKTypeCaster.h"

#include "vtkImageData.h"


namespace py = pybind11;

PYBIND11_VTK_TYPECASTER(vtkImageData)

PYBIND11_PLUGIN(_wrapping)
{
  py::module m("_wrapping", "tomviz wrapped classes");

  py::class_<OperatorPythonWrapper>(m, "OperatorPythonWrapper")
    .def("__init__",
         [](OperatorPythonWrapper& instance, void* op) {
           new (&instance) OperatorPythonWrapper(op);
         })
    .def_property_readonly("canceled", &OperatorPythonWrapper::canceled)
    .def_property("progress_maximum",
                  &OperatorPythonWrapper::totalProgressSteps,
                  &OperatorPythonWrapper::setTotalProgressSteps)
    .def_property("progress_value", &OperatorPythonWrapper::progressStep,
                  &OperatorPythonWrapper::setProgressStep)
    .def_property("progress_message", &OperatorPythonWrapper::progressMessage,
                  &OperatorPythonWrapper::setProgressMessage)
    .def_property("progress_data", &OperatorPythonWrapper::progressData,
                  &OperatorPythonWrapper::setProgressData);

  return m.ptr();
}
