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
    from tomviz.io.formats.matlab import MatlabReader
    # from tomviz.io.formats.plaintext import PlainTextReader
    reader_classes = [
        NumpyReader,
        MatlabReader,
        # PlainTextReader
    ]
    file_types = []
    for reader_class in reader_classes:
        file_types.append(
            [
                reader_class.file_type().display_name,
                reader_class.file_type().extensions,
                reader_class
            ]
        )
    return file_types


def get_reader_instance(reader_class):
    return reader_class()


def execute_reader(obj, path):
    return obj.read(path)


def list_python_writers():
    from tomviz.io.formats.numpy import NumpyWriter
    writer_classes = [
        NumpyWriter,
    ]
    file_types = []
    for writer_class in writer_classes:
        file_types.append(
            [
                writer_class.file_type().display_name,
                writer_class.file_type().extensions,
                writer_class
            ]
        )
    return file_types


def get_writer_instance(writer_class):
    return writer_class()


def execute_writer(obj, path, data):
    obj.write(path, data)
