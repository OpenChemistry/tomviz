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
#ifndef tomvizEmdFormat_h
#define tomvizEmdFormat_h

#include <string>

class vtkImageData;

namespace tomviz {

class DataSource;

class EmdFormat
{
public:
  EmdFormat();
  ~EmdFormat();

  bool read(const std::string& fileName, vtkImageData* data);
  bool write(const std::string& fileName, DataSource* source);
  bool write(const std::string& fileName, vtkImageData* image);

private:
  class Private;
  Private* d;
};
} // namespace tomviz

#endif // tomvizEmdFormat_h
