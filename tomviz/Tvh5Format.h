/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizTvh5Format_h
#define tomvizTvh5Format_h

#include <string>

class QJsonObject;

namespace h5 {
class H5ReadWrite;
}

namespace tomviz {

class DataSource;
class Operator;

class Tvh5Format
{
public:
  static bool write(const std::string& fileName);
  static bool read(const std::string& fileName);

private:
  // Load a data source from data in a Tvh5 file
  // If the active data source is found, it is set to @param active
  static bool loadDataSource(h5::H5ReadWrite& reader,
                             const QJsonObject& dsObject, DataSource** active,
                             Operator* parent = nullptr);
};
} // namespace tomviz

#endif // tomvizTvh5Format_h
