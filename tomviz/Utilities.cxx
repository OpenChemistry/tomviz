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
#include "Utilities.h"

#include "DataSource.h"
#include "vtkNew.h"
#include "vtkPVArrayInformation.h"
#include "vtkPVDataInformation.h"
#include "vtkPVDataSetAttributesInformation.h"
#include "vtkPVXMLElement.h"
#include "vtkPVXMLParser.h"
#include "vtkSmartPointer.h"
#include "vtkSMNamedPropertyIterator.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMTransferFunctionProxy.h"
#include "vtkStringList.h"

#include <sstream>

namespace tomviz
{

//---------------------------------------------------------------------------
bool serialize(vtkSMProxy* proxy, pugi::xml_node& out,
               const QStringList& properties)
{
  if (!proxy)
    {
    return false;
    }

  vtkSmartPointer<vtkSMNamedPropertyIterator> iter;
  if (properties.size() > 0)
    {
    vtkNew<vtkStringList> pnames;
    foreach (const QString& str, properties)
      {
      pnames->AddString(str.toLatin1().data());
      }
    iter = vtkSmartPointer<vtkSMNamedPropertyIterator>::New();
    iter->SetPropertyNames(pnames.GetPointer());
    iter->SetProxy(proxy);
    }

  // Save options state -- that's all we need.
  vtkSmartPointer<vtkPVXMLElement> elem;
  elem.TakeReference(proxy->SaveXMLState(NULL, iter.GetPointer()));

  std::ostringstream stream;
  elem->PrintXML(stream, vtkIndent());

  pugi::xml_document document;
  if (!document.load(stream.str().c_str()))
    {
    qCritical("Failed to convert from vtkPVXMLElement to pugi::xml_document");
    return false;
    }
  out.append_copy(document.first_child());
  return true;
}

//---------------------------------------------------------------------------
bool deserialize(vtkSMProxy* proxy, const pugi::xml_node& in,
                 vtkSMProxyLocator* locator)
{
  if (!proxy)
    {
    return false;
    }

  if (!in || !in.first_child())
    {
    // empty state loaded.
    return true;
    }

  std::ostringstream stream;
  in.first_child().print(stream);
  vtkNew<vtkPVXMLParser> parser;
  if (!parser->Parse(stream.str().c_str()))
    {
    return false;
    }
  if (proxy->LoadXMLState(parser->GetRootElement(), locator) != 0)
    {
    proxy->UpdateVTKObjects();
    return true;
    }
  return false;
}

//---------------------------------------------------------------------------
vtkPVArrayInformation* scalarArrayInformation(vtkSMSourceProxy* proxy)
{
  vtkPVDataInformation* dinfo = proxy->GetDataInformation();
  return dinfo? dinfo->GetPointDataInformation()->GetAttributeInformation(
    vtkDataSetAttributes::SCALARS) : NULL;
}


//---------------------------------------------------------------------------
bool rescaleColorMap(vtkSMProxy* colorMap, DataSource* dataSource)
{
  // rescale the color/opacity maps for the data source.
  vtkSMProxy* cmap = colorMap;
  vtkSMProxy* omap = vtkSMPropertyHelper(cmap, "ScalarOpacityFunction").GetAsProxy();
  vtkPVArrayInformation* ainfo = tomviz::scalarArrayInformation(dataSource->producer());
  if (ainfo != NULL && vtkSMPropertyHelper(cmap, "LockScalarRange").GetAsInt() == 0)
    {
    // assuming single component arrays.
    Q_ASSERT(ainfo->GetNumberOfComponents() == 1);
    vtkSMTransferFunctionProxy::RescaleTransferFunction(cmap, ainfo->GetComponentRange(0));
    vtkSMTransferFunctionProxy::RescaleTransferFunction(omap, ainfo->GetComponentRange(0));
    return true;
    }
  return false;
}


}
