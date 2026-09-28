/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelinePortType_h
#define tomvizPipelinePortType_h

#include <QFlags>
#include <QString>

namespace tomviz {
namespace pipeline {

/// Port data types.
///
/// Volume-like hierarchy:
///   ImageData  — any volumetric data (base type)
///   TiltSeries — volumetric data WITH tilt angles  (subtype of ImageData)
///   Volume     — volumetric data WITHOUT tilt angles (subtype of ImageData)
///   LabelMap   — categorical/segmentation volume    (subtype of ImageData)
enum class PortType
{
  None = 0,
  ImageData = 1,
  TiltSeries = 2,
  Volume = 4,
  LabelMap = 8,
  Image = 16,
  Scalar = 32,
  Array = 64,
  Table = 128,
  Molecule = 256
};

Q_DECLARE_FLAGS(PortTypes, PortType)
Q_DECLARE_OPERATORS_FOR_FLAGS(PortTypes)

/// Canonical list of all concrete port types. Iterate this instead of
/// hand-rolling brace-enum loops at call sites.
inline constexpr PortType kAllPortTypes[] = {
  PortType::ImageData, PortType::TiltSeries, PortType::Volume,
  PortType::LabelMap,  PortType::Image,      PortType::Scalar,
  PortType::Array,     PortType::Table,      PortType::Molecule
};

/// Returns the base type of @a t, or PortType::None if @a t has no base.
/// TiltSeries, Volume, and LabelMap all derive from ImageData.
inline PortType baseType(PortType t)
{
  switch (t) {
    case PortType::TiltSeries:
    case PortType::Volume:
    case PortType::LabelMap:
      return PortType::ImageData;
    default:
      return PortType::None;
  }
}

/// True if @a type is any volume-like type (ImageData or any subtype of it).
inline bool isVolumeType(PortType type)
{
  return type == PortType::ImageData ||
         baseType(type) == PortType::ImageData;
}

/// True if @a types accepts at least one volume-like type. Use this
/// rather than testFlag(ImageData) when asking "does this input take a
/// volume of some sort": an input narrowed to a single subtype (a
/// LabelMap-only sink, say) accepts volumes without accepting the base.
inline bool acceptsVolumeType(PortTypes types)
{
  for (PortType type : kAllPortTypes) {
    if (isVolumeType(type) && types.testFlag(type)) {
      return true;
    }
  }
  return false;
}

/// Subtype-aware compatibility check.
///
/// Returns true if an output of @a outputType can connect to an input that
/// accepts @a acceptedTypes. An input that accepts a base type also accepts
/// any of its subtypes.
inline bool isPortTypeCompatible(PortType outputType, PortTypes acceptedTypes)
{
  if (acceptedTypes.testFlag(outputType)) {
    return true;
  }
  PortType base = baseType(outputType);
  return base != PortType::None && acceptedTypes.testFlag(base);
}

/// PortType ↔ string conversions used by the state-file schema.
inline QString portTypeToString(PortType type)
{
  switch (type) {
    case PortType::None:
      return QStringLiteral("None");
    case PortType::ImageData:
      return QStringLiteral("ImageData");
    case PortType::TiltSeries:
      return QStringLiteral("TiltSeries");
    case PortType::Volume:
      return QStringLiteral("Volume");
    case PortType::LabelMap:
      return QStringLiteral("LabelMap");
    case PortType::Image:
      return QStringLiteral("Image");
    case PortType::Scalar:
      return QStringLiteral("Scalar");
    case PortType::Array:
      return QStringLiteral("Array");
    case PortType::Table:
      return QStringLiteral("Table");
    case PortType::Molecule:
      return QStringLiteral("Molecule");
  }
  return QStringLiteral("None");
}

inline PortType portTypeFromString(const QString& s)
{
  if (s == QLatin1String("ImageData"))
    return PortType::ImageData;
  if (s == QLatin1String("TiltSeries"))
    return PortType::TiltSeries;
  if (s == QLatin1String("Volume"))
    return PortType::Volume;
  if (s == QLatin1String("LabelMap"))
    return PortType::LabelMap;
  if (s == QLatin1String("Image"))
    return PortType::Image;
  if (s == QLatin1String("Scalar"))
    return PortType::Scalar;
  if (s == QLatin1String("Array"))
    return PortType::Array;
  if (s == QLatin1String("Table"))
    return PortType::Table;
  if (s == QLatin1String("Molecule"))
    return PortType::Molecule;
  return PortType::None;
}

} // namespace pipeline
} // namespace tomviz

#endif
