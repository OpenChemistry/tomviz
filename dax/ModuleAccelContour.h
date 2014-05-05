/******************************************************************************

  This source file is part of the TEM tomography project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#ifndef __ModuleAccelContour_h
#define __ModuleAccelContour_h

#include "Module.h"

#include <QThread>
#include <QScopedPointer>
#include "vtkImageData.h"
#include "vtkSmartPointer.h"

namespace TEM
{

//forward declare the acceleration data structure
namespace accel { class SubdividedVolume; }

class ContourWorker : public QThread
{
  Q_OBJECT
public:
  vtkSmartPointer<vtkImageData> input;

  //required to be in implementing since we are using a forward declared
  //class inside a QScopedPointer
  ContourWorker(QObject *p = 0);

  //required to be in implementing since we are using a forward declared
  //class inside a QScopedPointer
  ~ContourWorker();

public slots:
  void computeContour(double isoValue);

signals:
  void computed(int);

private:
  void run();

  template <class IteratorType>
  void createSearchStructure(IteratorType begin, IteratorType end);

  template <class IteratorType>
  bool contour(double v, const IteratorType begin, const IteratorType end);

  QScopedPointer<TEM::accel::SubdividedVolume> volume;
  std::size_t numSubGridsPerDim;
};

class ModuleAccelContour : public Module
{
  Q_OBJECT

  typedef Module Superclass;

public:
  ModuleAccelContour(QObject* parent=NULL);
  virtual ~ModuleAccelContour();

  virtual QString label() const { return  "Contour"; }
  virtual QIcon icon() const;
  virtual bool initialize(vtkSMSourceProxy* dataSource, vtkSMViewProxy* view);
  virtual bool finalize();
  virtual bool setVisibility(bool val);

private:
  Q_DISABLE_COPY(ModuleAccelContour)

  ContourWorker Worker;
};

}

#endif
