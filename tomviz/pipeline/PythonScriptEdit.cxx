/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */
#include "PythonScriptEdit.h"

#include <pqPythonSyntaxHighlighter.h>

#include <QFontDatabase>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>

namespace {

// Copy character formatting (syntax colors) from src into dst without
// modifying dst's text.  Walks blocks in parallel, skipping blank lines
// that only exist in one document (the HTML round-trip can lose them).
void applySyntaxFormatting(QTextDocument* dst, const QTextDocument& src)
{
  QTextCursor cursor(dst);
  cursor.beginEditBlock();
  QTextBlock dstBlock = dst->begin();
  QTextBlock srcBlock = src.begin();
  while (dstBlock.isValid() && srcBlock.isValid()) {
    if (dstBlock.text() == srcBlock.text()) {
      for (auto it = srcBlock.begin(); !it.atEnd(); ++it) {
        QTextFragment frag = it.fragment();
        int start =
          dstBlock.position() + frag.position() - srcBlock.position();
        cursor.setPosition(start);
        cursor.setPosition(start + frag.length(),
                           QTextCursor::KeepAnchor);
        cursor.setCharFormat(frag.charFormat());
      }
      dstBlock = dstBlock.next();
      srcBlock = srcBlock.next();
    } else if (dstBlock.text().isEmpty()) {
      dstBlock = dstBlock.next();
    } else if (srcBlock.text().isEmpty()) {
      srcBlock = srcBlock.next();
    } else {
      dstBlock = dstBlock.next();
      srcBlock = srcBlock.next();
    }
  }
  cursor.endEditBlock();
}

} // namespace

namespace tomviz {
namespace pipeline {

PythonScriptEdit::PythonScriptEdit(QWidget* parent) : QTextEdit(parent)
{
  setLineWrapMode(QTextEdit::NoWrap);

  auto* highlighter = new pqPythonSyntaxHighlighter(this, *this);

  // Set font after the highlighter ctor, which sets QFont("Monospace")
  setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

  // Wire up highlighting ourselves instead of ConnectHighligter(), which
  // uses setHtml() to replace the whole document — that loses blank lines.
  // Instead we parse the HTML into a temp document and copy only the
  // character formatting (colors) into the real document via
  // applySyntaxFormatting(), leaving the text untouched.
  auto* rehighlightTimer = new QTimer(this);
  rehighlightTimer->setSingleShot(true);
  rehighlightTimer->setInterval(0);

  connect(this, &QTextEdit::textChanged, rehighlightTimer,
          [rehighlightTimer]() { rehighlightTimer->start(); });

  connect(rehighlightTimer, &QTimer::timeout, this, [this, highlighter]() {
    const QString html = highlighter->Highlight(toPlainText());
    if (html.isEmpty()) {
      return;
    }
    QTextDocument tempDoc;
    tempDoc.setHtml(html);
    const bool blocked = blockSignals(true);
    applySyntaxFormatting(document(), tempDoc);
    blockSignals(blocked);
  });
}

} // namespace pipeline
} // namespace tomviz
