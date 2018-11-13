/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "Variant.h"

using namespace std;

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

Variant::Variant(const string& str) : m_type(Variant::STRING)
{
  new (&m_value.stringVal) string(str);
}

Variant::Variant(const vector<Variant>& l) : m_type(LIST)
{
  new (&m_value.listVal) vector<Variant>(l);
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
    m_value.stringVal.~string();
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

string Variant::toString() const
{
  return m_value.stringVal;
}

vector<Variant> Variant::toList() const
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
      new (&m_value.stringVal) string(v.toString());
      break;
    case LIST:
      new (&m_value.listVal) vector<Variant>(v.toList());
      break;
  }
}
} // namespace tomviz
