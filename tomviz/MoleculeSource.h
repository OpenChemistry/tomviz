/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#ifndef tomvizMoleculeSource_h
#define tomvizMoleculeSource_h

#include <QObject>

#include <QJsonObject>

class vtkMolecule;

namespace tomviz {
class Pipeline;

class MoleculeSource : public QObject
{
  Q_OBJECT

  public:
    MoleculeSource(vtkMolecule* molecule, QObject* parent = nullptr);
    ~MoleculeSource() override;

    /// Save the state out.
    QJsonObject serialize() const;
    bool deserialize(const QJsonObject& state);

    /// Set the file names.
    void setFileNames(const QStringList fileNames);
    void setFileName(QString label);

    /// Returns the list of files used to load the atoms.
    QStringList fileNames() const;
    QString fileName() const;

    /// Set the label for the data source.
    void setLabel(const QString& label);

    /// Returns the name of the filename used from the originalDataSource.
    QString label() const;

  private:
    QJsonObject m_json;
    vtkMolecule* m_molecule;

};

} // namespace tomviz

#endif