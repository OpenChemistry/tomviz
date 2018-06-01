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


import inspect
import pkgutil

import tomviz.io
import tomviz.io.formats


def list_python_readers():

    readers = []
    for importer, name, _ in pkgutil.iter_modules(tomviz.io.formats.__path__):
        m = importer.find_module(name).load_module(name)
        for _, c in inspect.getmembers(m, inspect.isclass):
            if inspect.getmodule(c) is m:
                if issubclass(c, tomviz.io.Reader):
                    readers.append(
                        [
                            c.file_type().display_name,
                            c.file_type().extensions,
                            c
                        ]
                    )
    return readers


def create_reader_instance(reader_class):
    return reader_class()


def execute_reader(obj, path):
    return obj.read(path)


def list_python_writers():
    writers = []
    for importer, name, _ in pkgutil.iter_modules(tomviz.io.formats.__path__):
        m = importer.find_module(name).load_module(name)
        for _, c in inspect.getmembers(m, inspect.isclass):
            if inspect.getmodule(c) is m:
                if issubclass(c, tomviz.io.Writer):
                    writers.append(
                        [
                            c.file_type().display_name,
                            c.file_type().extensions,
                            c
                        ]
                    )
    return writers


def create_writer_instance(writer_class):
    return writer_class()


def execute_writer(obj, path, data):
    obj.write(path, data)
