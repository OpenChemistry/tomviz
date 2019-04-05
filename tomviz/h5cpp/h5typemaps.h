/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizH5TypeMaps_h
#define tomvizH5TypeMaps_h

#include <map>

#include "h5capi.h"
#include "h5readwrite.h" // This is included only for the "DataType" enum

namespace h5 {

template <typename T>
struct BasicTypeToH5;

// Because these H5T_* macros are actually functions,
// we have to retrieve them like this, or we get
// compile errors.
template <>
struct BasicTypeToH5<char>
{
  static hid_t dataTypeId() { return H5T_STD_I8LE; }
  static hid_t memTypeId() { return H5T_NATIVE_CHAR; }
};

template <>
struct BasicTypeToH5<short>
{
  static hid_t dataTypeId() { return H5T_STD_I16LE; }
  static hid_t memTypeId() { return H5T_NATIVE_SHORT; }
};

template <>
struct BasicTypeToH5<int>
{
  static hid_t dataTypeId() { return H5T_STD_I32LE; }
  static hid_t memTypeId() { return H5T_NATIVE_INT; }
};

template <>
struct BasicTypeToH5<long long>
{
  static hid_t dataTypeId() { return H5T_STD_I64LE; }
  static hid_t memTypeId() { return H5T_NATIVE_LLONG; }
};

template <>
struct BasicTypeToH5<unsigned char>
{
  static hid_t dataTypeId() { return H5T_STD_U8LE; }
  static hid_t memTypeId() { return H5T_NATIVE_UCHAR; }
};

template <>
struct BasicTypeToH5<unsigned short>
{
  static hid_t dataTypeId() { return H5T_STD_U16LE; }
  static hid_t memTypeId() { return H5T_NATIVE_USHORT; }
};

template <>
struct BasicTypeToH5<unsigned int>
{
  static hid_t dataTypeId() { return H5T_STD_U32LE; }
  static hid_t memTypeId() { return H5T_NATIVE_UINT; }
};

template <>
struct BasicTypeToH5<unsigned long long>
{
  static hid_t dataTypeId() { return H5T_STD_U64LE; }
  static hid_t memTypeId() { return H5T_NATIVE_ULLONG; }
};

template <>
struct BasicTypeToH5<float>
{
  static hid_t dataTypeId() { return H5T_IEEE_F32LE; }
  static hid_t memTypeId() { return H5T_NATIVE_FLOAT; }
};

template <>
struct BasicTypeToH5<double>
{
  static hid_t dataTypeId() { return H5T_IEEE_F64LE; }
  static hid_t memTypeId() { return H5T_NATIVE_DOUBLE; }
};

// Map of H5 types to our own enum class DataType
// Key comparison must be done with H5Tequal
static const std::map<hid_t, H5ReadWrite::DataType> H5ToDataType = {
  { H5T_STD_I8LE, H5ReadWrite::DataType::Int8 },
  { H5T_STD_I16LE, H5ReadWrite::DataType::Int16 },
  { H5T_STD_I32LE, H5ReadWrite::DataType::Int32 },
  { H5T_STD_I64LE, H5ReadWrite::DataType::Int64 },
  { H5T_STD_U8LE, H5ReadWrite::DataType::UInt8 },
  { H5T_STD_U16LE, H5ReadWrite::DataType::UInt16 },
  { H5T_STD_U32LE, H5ReadWrite::DataType::UInt32 },
  { H5T_STD_U64LE, H5ReadWrite::DataType::UInt64 },
  { H5T_IEEE_F32LE, H5ReadWrite::DataType::Float },
  { H5T_IEEE_F64LE, H5ReadWrite::DataType::Double }
};

static const std::map<H5ReadWrite::DataType, hid_t> DataTypeToH5DataType = {
  { H5ReadWrite::DataType::Int8, H5T_STD_I8LE },
  { H5ReadWrite::DataType::Int16, H5T_STD_I16LE },
  { H5ReadWrite::DataType::Int32, H5T_STD_I32LE },
  { H5ReadWrite::DataType::Int64, H5T_STD_I64LE },
  { H5ReadWrite::DataType::UInt8, H5T_STD_U8LE },
  { H5ReadWrite::DataType::UInt16, H5T_STD_U16LE },
  { H5ReadWrite::DataType::UInt32, H5T_STD_U32LE },
  { H5ReadWrite::DataType::UInt64, H5T_STD_U64LE },
  { H5ReadWrite::DataType::Float, H5T_IEEE_F32LE },
  { H5ReadWrite::DataType::Double, H5T_IEEE_F64LE }
};

static const std::map<H5ReadWrite::DataType, hid_t> DataTypeToH5MemType = {
  { H5ReadWrite::DataType::Int8, H5T_NATIVE_CHAR },
  { H5ReadWrite::DataType::Int16, H5T_NATIVE_SHORT },
  { H5ReadWrite::DataType::Int32, H5T_NATIVE_INT },
  { H5ReadWrite::DataType::Int64, H5T_NATIVE_LLONG },
  { H5ReadWrite::DataType::UInt8, H5T_NATIVE_UCHAR },
  { H5ReadWrite::DataType::UInt16, H5T_NATIVE_USHORT },
  { H5ReadWrite::DataType::UInt32, H5T_NATIVE_UINT },
  { H5ReadWrite::DataType::UInt64, H5T_NATIVE_ULLONG },
  { H5ReadWrite::DataType::Float, H5T_NATIVE_FLOAT },
  { H5ReadWrite::DataType::Double, H5T_NATIVE_DOUBLE }
};

} // end namespace h5

#endif // tomvizH5TypeMaps
