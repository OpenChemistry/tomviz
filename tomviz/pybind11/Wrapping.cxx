/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

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
    .def(py::init([](void* op) { return new OperatorPythonWrapper(op); }))
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
