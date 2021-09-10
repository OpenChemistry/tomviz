/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ctvlib.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "eigenConversion.h"

namespace py = pybind11;

PYBIND11_PLUGIN(ctvlib)
{
  py::module m("ctvlib", "C++ Tomography Reconstruction Scripts");

  py::class_<ctvlib>(m, "ctvlib")
    .def(pybind11::init<int, int, int>())
    .def("Nslice", &ctvlib::get_Nslice, "Get Nslice")
    .def("Nray", &ctvlib::get_Nray, "Get Nray")
    .def("set_tilt_series", &ctvlib::set_tilt_series,
         "Pass the Projections to C++ Object")
    .def("initialize_recon_copy", &ctvlib::initialize_recon_copy,
         "Initialize Recon Copy")
    .def("initialize_tv_recon", &ctvlib::initialize_tv_recon,
         "Initialize TV Recon")
    .def("update_proj_angles", &ctvlib::update_proj_angles,
         "Update Algorithm with New projections")
    .def("get_recon", &ctvlib::get_recon, "Return the Reconstruction to Python")
    .def("ART", &ctvlib::ART, "ART Reconstruction")
    .def("randART", &ctvlib::randART, "Stochastic ART Reconstruction")
    .def("SIRT", &ctvlib::SIRT, "SIRT Reconstruction")
    .def("lipschits", &ctvlib::lipschits, "Calculate Lipschitz Constant")
    .def("row_inner_product", &ctvlib::normalization,
         "Calculate the Row Inner Product for Measurement Matrix")
    .def("positivity", &ctvlib::positivity, "Remove Negative Elements")
    .def("forward_projection", &ctvlib::forward_projection,
         "Forward Projection")
    .def("load_A", &ctvlib::loadA, "Load Measurement Matrix Created By Python")
    .def("copy_recon", &ctvlib::copy_recon, "Copy the reconstruction")
    .def("matrix_2norm", &ctvlib::matrix_2norm,
         "Calculate L2-Norm of Reconstruction")
    .def("data_distance", &ctvlib::data_distance,
         "Calculate L2-Norm of Projection (aka Vectors)")
    .def("tv_gd", &ctvlib::tv_gd_3D, "3D TV Gradient Descent")
    .def("get_projections", &ctvlib::get_projections,
         "Return the projection matrix to python")
    .def("restart_recon", &ctvlib::restart_recon,
         "Set all the Slices Equal to Zero");

  return m.ptr();
}
