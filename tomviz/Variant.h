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
#ifndef tomvizVariant_h
#define tomvizVariant_h

#include <iostream>
#include <string>
#include <vector>

namespace tomviz {

class Variant
{

public:
  enum Type
  {
    INVALID,
    INTEGER,
    DOUBLE,
    BOOL,
    STRING,
    LIST
  };

  Variant();
  Variant(const std::string& str);
  Variant(const std::vector<Variant>& l);
  Variant(int i);
  Variant(double d);
  Variant(bool b);
  Variant(const Variant& v);
  ~Variant();

  Variant& operator=(const Variant& v);

  bool toBool() const;
  int toInteger() const;
  double toDouble() const;
  std::string toString() const;
  std::vector<Variant> toList() const;
  Type type() const;

private:
  Type m_type;
  union VariantUnion
  {
    int integerVal;
    double doubleVal;
    bool boolVal;
    std::string stringVal;
    std::vector<Variant> listVal;

    VariantUnion() {}

    ~VariantUnion() {}
  };

  VariantUnion m_value;
  void copy(const Variant& v);
};
} // namespace tomviz

#endif
