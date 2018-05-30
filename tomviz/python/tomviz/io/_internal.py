# -*- coding: utf-8 -*-

###############################################################################
#
#  This source file is part of the tomviz project.
#
#  Copyright Kitware, Inc.
#
#  This source code is released under the New BSD License, (the "License").
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
###############################################################################

def list_python_readers():
    from tomviz.io.formats.numpy import NumpyReader
    from tomviz.io.formats.plaintext import PlainTextReader
    reader_classes = [
        NumpyReader,
        PlainTextReader
    ]
    file_types = []
    for reader_class in reader_classes:
        file_types.append([reader_class.file_type().display_name, reader_class.file_type().extensions, reader_class])
    return file_types

def get_reader_instance(reader_class):
    return reader_class()

def execute_reader(obj, path):
    return obj.read(path)

def execute_writer(obj, path, data):
    obj.write(path, data)

