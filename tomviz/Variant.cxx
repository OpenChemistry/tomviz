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
#include "Variant.h"

namespace tomviz {

Variant::Variant()
{
  m_type = Variant::INVALID;
}

Variant::Variant(const Variant& v)
{
  copy(v);
}

Variant& Variant::operator=(const Variant& v)
{
  this->~Variant();
  copy(v);

  return *this;
}

Variant::Variant(const std::string& str) : m_type(Variant::STRING)
{
  new (&m_value.stringVal) std::string(str);
}

Variant::Variant(const std::vector<Variant>& l) : m_type(LIST)
{
  new (&m_value.listVal) std::vector<Variant>(l);
}

Variant::Variant(int i) : m_type(Variant::INTEGER)
{
  m_value.integerVal = i;
}

Variant::Variant(double d) : m_type(Variant::DOUBLE)
{
  m_value.doubleVal = d;
}

Variant::Variant(bool b) : m_type(Variant::BOOL)
{
  m_value.boolVal = b;
}

Variant::~Variant()
{
  if (m_type == Variant::STRING) {
    m_value.stringVal.std::string::~string();
  } else if (m_type == LIST) {
    m_value.listVal.~vector<Variant>();
  }
}

bool Variant::toBool() const
{
  return m_value.boolVal;
}

int Variant::toInteger() const
{
  return m_value.integerVal;
}

double Variant::toDouble() const
{
  return m_value.doubleVal;
}

std::string Variant::toString() const
{
  return m_value.stringVal;
}

std::vector<Variant> Variant::toList() const
{
  return m_value.listVal;
}

Variant::Type Variant::type() const
{
  return m_type;
}

void Variant::copy(const Variant& v)
{
  m_type = v.type();
  switch (v.type()) {
    case INVALID:
      // do nothing
      break;
    case INTEGER:
      m_value.integerVal = v.toInteger();
      break;
    case DOUBLE:
      m_value.doubleVal = v.toDouble();
      break;
    case BOOL:
      m_value.boolVal = v.toBool();
      break;
    case STRING:
      new (&m_value.stringVal) std::string(v.toString());
      break;
    case LIST:
      new (&m_value.listVal) std::vector<Variant>(v.toList());
      break;
  }
}
}
