/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizHIDCloser_h
#define tomvizHIDCloser_h

#include "h5capi.h"

namespace h5 {

class HIDCloser
{
public:
  explicit HIDCloser(hid_t value, herr_t (*closer)(hid_t))
    : m_value(value), m_closer(closer)
  {}

  bool valueIsValid() { return m_value >= 0; }

  hid_t value() { return m_value; }

  void reset(hid_t value)
  {
    close();
    m_value = value;
  }

  herr_t close()
  {
    herr_t result = 0;
    if (valueIsValid() && m_closer != nullptr) {
      result = m_closer(m_value);
      m_value = H5I_INVALID_HID;
    }
    return result;
  }

  HIDCloser(const HIDCloser&) = delete;
  HIDCloser& operator=(const HIDCloser&) = delete;

  HIDCloser(HIDCloser&& other) noexcept { *this = std::move(other); }

  HIDCloser& operator=(HIDCloser&& other) noexcept
  {
    if (this != &other) {
      close();
      m_value = other.m_value;
      other.m_value = H5I_INVALID_HID;
    }
    return *this;
  }

  ~HIDCloser() { close(); }

private:
  hid_t m_value = H5I_INVALID_HID;
  herr_t (*m_closer)(hid_t) = nullptr;
};

} // namespace h5

#endif // tomvizHIDCloser
