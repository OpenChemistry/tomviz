#ifndef pybind_extension_vtk_source_VTKTypeCaster_h
#define pybind_extension_vtk_source_VTKTypeCaster_h

#include <pybind11/pybind11.h>

#include <type_traits>

#include "vtkObjectBase.h"
#include "vtkPythonUtil.h"

#define PYBIND11_VTK_TYPECASTER(VTK_OBJ)                                \
  namespace pybind11 {                                                  \
    namespace detail {                                                  \
    template <>                                                         \
  struct type_caster<VTK_OBJ> {                                         \
  protected:                                                            \
VTK_OBJ *value;                                                         \
public:                                                                 \
static PYBIND11_DESCR name() { return type_descr(_(#VTK_OBJ)); }        \
static handle cast(const VTK_OBJ *src, return_value_policy policy,      \
                   handle parent) {                                     \
  return cast(*src, policy, parent);                                    \
}                                                                       \
operator VTK_OBJ *() { return value; }                                  \
operator VTK_OBJ &() { return *value; }                                 \
template <typename _T> using cast_op_type =                             \
  pybind11::detail::cast_op_type<_T>;                                   \
bool load(handle src, bool) {                                           \
  value = dynamic_cast< VTK_OBJ *>(                                     \
    vtkPythonUtil::GetPointerFromObject(src.ptr(), #VTK_OBJ));          \
  if (!value) {                                                         \
    PyErr_Clear();                                                      \
    throw reference_cast_error();                                       \
  }                                                                     \
  return value != nullptr;                                              \
}                                                                       \
static handle cast(const VTK_OBJ& src, return_value_policy, handle) {   \
  return vtkPythonUtil::GetObjectFromPointer(                           \
    const_cast< VTK_OBJ *>(&src));                                      \
}                                                                       \
};                                                                      \
}}

#endif
