/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizFileReader_h
#define tomvizFileReader_h

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <vtkSmartPointer.h>

class vtkImageData;

namespace tomviz {

/// Result of a headless file read via readImageData().
struct ReadResult
{
  /// Populated on success, null on failure.
  vtkSmartPointer<vtkImageData> imageData;

  /// Non-empty for formats that carry them (DataExchange, FXI).
  QVector<double> tiltAngles;

  /// Non-null for formats with reference frames (DataExchange).
  vtkSmartPointer<vtkImageData> darkData;
  vtkSmartPointer<vtkImageData> whiteData;

  /// For ParaView-reader-backed reads, the reader's XML name and
  /// properties so a source node can round-trip through state files.
  QJsonObject readerProperties;

  /// For tvh5 reads, the internal /tomviz_datasources/<id> path used.
  QString tvh5NodePath;
};

/// Read image data from @a fileNames headlessly. No dialogs are shown,
/// no nodes are created, ActiveObjects is only touched when a ParaView
/// reader is explicitly requested via @a options["reader"].
///
/// Format dispatch:
///   .tvh5              -> EmdFormat::readNode (options["tvh5NodePath"] required)
///   .emd               -> EmdFormat::read
///   .h5 / .hspy / .nxs -> DataExchange / FXI / GenericHDF5Format (auto-detect)
///   *.ome.tif[f]       -> vtkOMETiffReader
///   .tif/.tiff/.png/.jpg/.jpeg/.vti/.mrc -> direct VTK readers
///   other              -> FileFormatManager Python reader if registered
///   fallback           -> ParaView reader if options["reader"] is supplied
///                         (otherwise ReadResult.imageData is null)
///
/// @a options keys used:
///   "tvh5NodePath"      (string) — required for .tvh5
///   "subsampleSettings" (object) — forwarded to HDF5 readers
///   "reader"            (object) — ParaView reader descriptor
///                                  ({"name": "...", ...properties})
ReadResult readImageData(const QStringList& fileNames,
                         const QJsonObject& options = QJsonObject());

} // namespace tomviz

#endif
