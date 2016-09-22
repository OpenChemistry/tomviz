#include <pybind11/pybind11.h>

#include "OperatorPython.h"

using namespace tomviz;

struct OperatorPythonWrapper
{
  OperatorPythonWrapper(OperatorPython* o) { this->op = o; };
  bool canceled() { return this->op->canceled(); }
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
    .def_property_readonly("canceled", &OperatorPythonWrapper::canceled);

  return m.ptr();
}
