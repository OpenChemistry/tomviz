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
#ifndef __Utilties_h
#define __Utilties_h

// Collection of miscellaneous utility functions.

#include "pqApplicationCore.h"
#include "pqProxy.h"
#include "pqServerManagerModel.h"
#include "vtkSMProxy.h"

#include <QFileInfo>

namespace TEM
{
  //===========================================================================
  // Functions for converting from pqProxy to vtkSMProxy and vice-versa.
  //===========================================================================

  //---------------------------------------------------------------------------
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

  //---------------------------------------------------------------------------
  /// convert a pqProxy to vtkSMProxy.
  inline vtkSMProxy* convert(pqProxy* pqproxy)
    {
    return pqproxy? pqproxy->getProxy() : NULL;
    }

  //===========================================================================
  // Functions for annotation proxies to help identification in MatViz.
  //===========================================================================

  //---------------------------------------------------------------------------
  // Annotate a proxy to be recognized as the data producer in the application.
  inline bool annotateDataProducer(vtkSMProxy* proxy, const char* filename)
    {
    if (proxy)
      {
      proxy->SetAnnotation("MatViz.Type", "DataSource");
      QFileInfo fileInfo(filename);
      proxy->SetAnnotation("MatViz.DataSource.FileName", filename);
      proxy->SetAnnotation("MatViz.Label", fileInfo.fileName().toAscii().data());
      return true;
      }
    return false;
    }
  inline bool annotateDataProducer(pqProxy* pqproxy, const char* filename)
    { return annotateDataProducer(convert(pqproxy), filename); }

  //---------------------------------------------------------------------------
  // Check if the proxy is a data producer.
  inline bool isDataProducer(vtkSMProxy* proxy)
    {
    return proxy &&
      proxy->HasAnnotation("MatViz.Type") &&
      (QString("DataSource") == proxy->GetAnnotation("MatViz.Type"));
    }
  inline bool isDataProducer(pqProxy* pqproxy)
    { return isDataProducer(convert(pqproxy)); }

  //---------------------------------------------------------------------------
  // Returns the MatViz label for a proxy, if any, otherwise simply returns the
  // XML label for it.
  inline QString label(vtkSMProxy* proxy)
    {
    if (proxy && proxy->HasAnnotation("MatViz.Label"))
      {
      return proxy->GetAnnotation("MatViz.Label");
      }
    return proxy? proxy->GetXMLLabel() : NULL;
    }
  inline QString label(pqProxy* proxy) { return label(convert(proxy)); }
}
#endif
