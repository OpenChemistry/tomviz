/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizUtilties_h
#define tomvizUtilties_h

// Collection of miscellaneous utility functions.

#include <pqApplicationCore.h>
#include <pqProxy.h>
#include <pqServerManagerModel.h>
#include <vtkSMProperty.h>
#include <vtkSMSourceProxy.h>
#include <vtkVariant.h>

#include <core/Variant.h>
#include <vtkVector.h>
#include <vtk_pugixml.h>

#include <QFileInfo>
#include <QJsonDocument>
#include <QStringList>
#include <QVariant>

class QTextStream;

#include <vector>

class pqAnimationScene;

class vtkColorTransferFunction;
class vtkDiscretizableColorTransferFunction;
class vtkImageSliceMapper;
class vtkCubeAxesActor;
class vtkImageData;
class vtkMolecule;
class vtkRenderer;
class vtkSMProxyLocator;
class vtkSMRenderViewProxy;
class vtkPVArrayInformation;
class vtkPiecewiseFunction;
class vtkTable;

class QDir;
class QLayout;
class QUrl;

namespace tomviz {

struct Attributes
{
  static const char* TYPE;
  static const char* DATASOURCE_FILENAME;
  static const char* LABEL;
  static const char* FILENAME;
};

//===========================================================================
// Functions for converting from pqProxy to vtkSMProxy and vice-versa.
//===========================================================================

/// Converts a vtkSMProxy to a pqProxy subclass by forwarding the call to
/// pqServerManagerModel instance.
template <class T>
T convert(vtkSMProxy* proxy)
{
  pqApplicationCore* appcore = pqApplicationCore::instance();
  Q_ASSERT(appcore);
  pqServerManagerModel* smmodel = appcore->getServerManagerModel();
  Q_ASSERT(smmodel);
  return smmodel->findItem<T>(proxy);
}

/// Convert a pqProxy to vtkSMProxy.
inline vtkSMProxy* convert(pqProxy* pqproxy)
{
  return pqproxy ? pqproxy->getProxy() : nullptr;
}

//===========================================================================
// Functions for annotation proxies to help identification in tomviz.
//===========================================================================

// Annotate a proxy to be recognized as the data producer in the application.
inline bool annotateDataProducer(vtkSMProxy* proxy, const char* filename)
{
  if (proxy) {
    proxy->SetAnnotation(Attributes::TYPE, "DataSource");
    QFileInfo fileInfo(filename);
    proxy->SetAnnotation(Attributes::DATASOURCE_FILENAME, filename);
    proxy->SetAnnotation(Attributes::LABEL,
                         fileInfo.fileName().toLatin1().data());
    return true;
  }
  return false;
}

inline bool annotateDataProducer(pqProxy* pqproxy, const char* filename)
{
  return annotateDataProducer(convert(pqproxy), filename);
}

//// Check if the proxy is a data producer.
// inline bool isDataProducer(vtkSMProxy* proxy)
//{
//  return proxy &&
//      proxy->HasAnnotation(Attributes::TYPE) &&
//      (QString("DataSource") == proxy->GetAnnotation(Attributes::TYPE));
//}
//
// inline bool isDataProducer(pqProxy* pqproxy)
//{
//  return isDataProducer(convert(pqproxy));
//}

// Returns the tomviz label for a proxy, if any, otherwise simply returns the
// XML label for it.
inline QString label(vtkSMProxy* proxy)
{
  if (proxy && proxy->HasAnnotation(Attributes::LABEL)) {
    return proxy->GetAnnotation(Attributes::LABEL);
  }
  return proxy ? proxy->GetXMLLabel() : nullptr;
}

inline QString label(pqProxy* proxy)
{
  return label(convert(proxy));
}

//// Making a function to serialize some ParaView proxies we are interested in.
QJsonObject serialize(vtkSMProxy* proxy);
bool deserialize(vtkSMProxy* proxy, const QJsonObject& json);

/// Serialize a proxy to a pugi::xml node.
bool serialize(vtkSMProxy* proxy, pugi::xml_node& out,
               const QStringList& properties = QStringList(),
               const QDir* relDir = nullptr);
bool deserialize(vtkSMProxy* proxy, const pugi::xml_node& in,
                 const QDir* relDir = nullptr,
                 vtkSMProxyLocator* locator = nullptr);

/// Serialize/deserialize a QVariant to a pugi::xml node
bool serialize(const QVariant& variant, pugi::xml_node& out);
bool deserialize(QVariant& variant, const pugi::xml_node& in);
/// Serialize/deserialize a QVariantMap to a pugi::xml node
bool serialize(const QVariantMap& map, pugi::xml_node& out);
bool deserialize(QVariantMap& map, const pugi::xml_node& in);
/// Serialize/deserialize a vtkPiecewiseFunction
bool serialize(vtkPiecewiseFunction* func, pugi::xml_node& out);
bool deserialize(vtkPiecewiseFunction* func, const pugi::xml_node& in);

QJsonObject serialize(vtkDiscretizableColorTransferFunction* func);
bool deserialize(vtkDiscretizableColorTransferFunction* func,
                 const QJsonObject& json);
QJsonObject serialize(vtkPiecewiseFunction* func);
bool deserialize(vtkPiecewiseFunction* func, const QJsonObject& json);

/// Returns the vtkPVArrayInformation for scalars array produced by the given
/// source proxy.
vtkPVArrayInformation* scalarArrayInformation(vtkSMSourceProxy* proxy);

/// Rescales the colorMap (and associated opacityMap) using the transformed data
/// range from the data source. This will respect the "LockScalarRange" property
/// on the colorMap i.e. if user locked the scalar range, it won't be rescaled.
bool rescaleColorMap(vtkSMProxy* colorMap, vtkSMSourceProxy* dataProxy);

/// Record @a values on the proxy's vector property @a name without
/// pushing them to the VTK object, for when that object already holds
/// exactly these values (the caller set them on it directly). The
/// property is left unmodified, so the next UpdateVTKObjects() skips
/// it. A push would replay the property's command per element; for a
/// transfer function's points that is AddPoint with a re-sort each
/// time, quadratic in the number of points and minutes at 100k.
void recordProxyValues(vtkSMProxy* proxy, const char* name,
                       const double* values, unsigned int count);

// Given the root of a file and an extension, reades the file fileName +
// extension and returns the content in a QString.
QString readInTextFile(const QString& fileName, const QString& extension);

// Given the name of a python script, find the script file and return the
// contents. This assumes that the given script is one of the built-in tomviz
// Python operator scripts.
QString readInPythonScript(const QString& scriptName);

// Given the name of an operator python script, find the JSON description
// file and return the contents. This assumes that the given script is one
// of the built-in tomviz python operator scripts.
QString readInJSONDescription(const QString& scriptName);

// Get the path for the Tomviz directory
QString userDataPath();

// Remove all camera cues from the animation scene.
// If a render view is provided, only camera cues associated with that
// render view will be removed.
void clearCameraCues(vtkSMRenderViewProxy* renderView = nullptr);

// Create a camera orbit animation for the given renderview around the given
// object
void createCameraOrbit(vtkSMSourceProxy* data,
                       vtkSMRenderViewProxy* renderView);

// Create a camera orbit animation for the given renderview around the current
// camera focal point.
void createCameraOrbit(vtkSMRenderViewProxy* renderView);

// Set the number of frames in the animation scene
void setAnimationNumberOfFrames(int numFrames);

/// The frame count tomviz gives an animation it starts itself.
constexpr int defaultAnimationFrames = 200;

/// Raise the scene to defaultAnimationFrames if it still holds too few
/// frames to show any motion. A scene nothing has configured holds one,
/// which plays as a single still and reads as the animation having done
/// nothing at all. Returns true if the count was changed.
bool ensureAnimationFrames();

// Set the animation time steps and set the play mode to "Snap to TimeSteps"
// This will also set the "TimeRange".
// The time steps must be sorted.
void snapAnimationToTimeSteps(const std::vector<double>& timeSteps);

// Set up the renderer to show the given slice in parallel projection
// This function attempts to zoom the renderer so that the enitire slice is
// visible while minimizing the empty regions of the view (zoom so the slice's
// target dimension barely fits in the view).
void setupRenderer(vtkRenderer* renderer, vtkImageSliceMapper* mapper,
                   vtkCubeAxesActor* axesActor = nullptr);

// Delete all widgets within a layout
void deleteLayoutContents(QLayout* layout);

Variant toVariant(const QVariant& value);
Variant toVariant(const QVariantList& value);
Variant toVariant(const QVariantMap& value);

QVariant toQVariant(const Variant& value);

/// Find common prefix for collection of file names
QString findPrefix(const QStringList& fileNames);

/// Convenience function to get the main widget (useful for dialog parenting).
QWidget* mainWidget();

/// Promote a top-level widget to a Qt::Tool window so it reliably floats above
/// the main window. On macOS a parented modeless QDialog is only hinted (not
/// guaranteed) to stack above its parent and can slip behind the main window
/// when the user clicks it; a Qt::Tool window floats above the application's
/// windows while leaving them interactive. Call this before the widget is
/// first shown. Any existing window hint flags (frameless, title-only, etc.)
/// are preserved.
void floatAboveMainWindow(QWidget* widget);

QJsonValue toJson(vtkVariant variant);
QJsonValue toJson(vtkSMProperty* prop);
bool setProperties(const QJsonObject& props, vtkSMProxy* proxy);
bool setProperty(const QJsonValue& value, vtkSMProperty* prop, int index = 0);
bool setProperty(const QJsonArray& array, vtkSMProperty* prop);

/// Write a vtkTable to json file
bool jsonToFile(const QJsonDocument& json);
QJsonDocument tableToJson(vtkTable* table);
QJsonDocument vectorToJson(const QVector<vtkVector2i> vector);

/// Write a vtkTable to csv file
QString tableToCsv(vtkTable* table);
bool csvToFile(const QString& csv);

/// Write a vtkMolecule to json file
bool moleculeToFile(vtkMolecule* molecule);

/// Write a vtkMolecule as XYZ to an explicit path, without prompting.
bool moleculeToXyzFile(vtkMolecule* molecule, const QString& fileName);
extern double offWhite[3];

/// Open a url in the user's default browser
void openUrl(const QString& link);
void openUrl(const QUrl& url);

/// Prepends the path with a local path if available.
/// Otherwise, prepends it with the remote help path.
/// If empty, just opens up the doc home page.
void openHelpUrl(const QString& path = "");

/// Rescale the points to be in a new range
bool vtkRescaleControlPoints(std::vector<vtkTuple<double, 4>>& cntrlPoints,
                             double rangeMin, double rangeMax);

void rescaleLut(vtkColorTransferFunction* lut, double rangeMin,
                double rangeMax);

/// Get the value of a voxel at the given world coordinates
double getVoxelValue(vtkImageData* data, const vtkVector3d& point,
                     vtkVector3i& ijk, bool& ok);

template <typename T>
QString getSizeNearestThousand(T num, bool labelAsBytes = false)
{
  char format = 'f';
  int prec = 1;

  QString ret;
  if (num < 1e3)
    ret = QString::number(num) + " ";
  else if (num < 1e6)
    ret = QString::number(num / 1e3, format, prec) + " K";
  else if (num < 1e9)
    ret = QString::number(num / 1e6, format, prec) + " M";
  else if (num < 1e12)
    ret = QString::number(num / 1e9, format, prec) + " G";
  else
    ret = QString::number(num / 1e12, format, prec) + " T";

  if (labelAsBytes)
    ret += "B";

  return ret;
}

void addPlaceholderNodes(vtkColorTransferFunction* lut, const double range[2]);
void removePlaceholderNodes(vtkColorTransferFunction* lut);

void addPlaceholderNodes(vtkPiecewiseFunction* opacity, const double range[2]);
void removePlaceholderNodes(vtkPiecewiseFunction* opacity);

double rescale(double val, double oldMin, double oldMax, double newMin,
               double newMax);
void rescaleNodes(vtkColorTransferFunction* lut, double newMin, double newMax);
void rescaleNodes(vtkPiecewiseFunction* opacity, double newMin, double newMax);
void removePointsOutOfRange(vtkColorTransferFunction* lut,
                            const double range[2]);
void removePointsOutOfRange(vtkPiecewiseFunction* opacity,
                            const double range[2]);

// Load a plugin by path
bool loadPlugin(QString path);

// Load plugins with the specified substring in the file name
bool loadPluginsWithSubstring(const QString& substring);
QStringList pluginsWithSubstring(const QString& substring);

// Is the looking glass plugin location set?
bool hasLookingGlassPlugin();

// Load the looking glass plugin
bool loadLookingGlassPlugin();

// Automatically load plugins specified in the TOMVIZ_PLUGIN_PATHS macro
bool loadPlugins();

/**
 * Swap the X and Z dimensions, spacing, and origin of the image without
 * actually modifying the data.
 */
void relabelXAndZAxes(vtkImageData* image);

/**
 * Parse scan IDs from a whitespace-delimited text stream. By default
 * the first column holds them; a '#' comment header naming the columns
 * (e.g. the ptycho output info file's "# Angle SID Version") selects
 * the SID column instead. Used by the ptycho and pyxrf dialogs.
 */
QStringList readSidsFromText(QTextStream& reader);

} // namespace tomviz

#endif
