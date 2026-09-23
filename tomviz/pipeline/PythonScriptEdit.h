/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */
#ifndef tomvizPipelinePythonScriptEdit_h
#define tomvizPipelinePythonScriptEdit_h

#include <QTextEdit>

namespace tomviz {
namespace pipeline {

/// A QTextEdit set up for Python source: fixed-pitch font, no wrapping,
/// and ParaView's syntax colouring re-applied shortly after every change
/// without touching the text itself. The node editor's Script tab and
/// the custom operator file editor both use it.
class PythonScriptEdit : public QTextEdit
{
  Q_OBJECT

public:
  explicit PythonScriptEdit(QWidget* parent = nullptr);

private:
  Q_DISABLE_COPY(PythonScriptEdit)
};

} // namespace pipeline
} // namespace tomviz

#endif
