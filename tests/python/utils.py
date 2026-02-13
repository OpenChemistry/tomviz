from pathlib import Path
from types import ModuleType
from typing import Callable
import importlib.util
import inspect
import shutil
import urllib.request
import zipfile

from tomviz.executor import OperatorWrapper
from tomviz.operators import Operator
from tomviz._internal import add_transform_decorators

OPERATOR_PATH = Path(__file__).parent.parent.parent / 'tomviz/python'


def add_decorators(func: Callable, operator_name: str) -> Callable:
    # Automatically add the decorators which would normally be
    # automatically added by Tomviz.
    json_path = OPERATOR_PATH / f'{operator_name}.json'
    op_dict = {}
    if json_path.exists():
        with open(json_path, 'rb') as rf:
            op_dict['description'] = rf.read()

    func = add_transform_decorators(func, op_dict)
    return func


def load_operator_module(operator_name: str) -> ModuleType:
    module_path = OPERATOR_PATH / f'{operator_name}.py'
    spec = importlib.util.spec_from_file_location(operator_name, module_path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    if hasattr(module, 'transform'):
        # Add the decorators
        module.transform = add_decorators(module.transform, operator_name)

    return module


def load_operator_class(operator_module: ModuleType) -> Operator | None:
    # Locate the operator class
    for v in operator_module.__dict__.values():
        if inspect.isclass(v) and issubclass(v, Operator):
            if hasattr(v, 'transform'):
                # Decorate at the class level
                name = operator_module.__name__
                v.transform = add_decorators(v.transform, name)

            # Instantiate and set up wrapper
            operator = v()
            operator._operator_wrapper = OperatorWrapper()

            return operator


def download_file(url: str, destination: str):
    filename = Path('downloaded_file')
    if filename.exists():
        filename.unlink()

    last_progress = -1

    def download_progress_hook(block_num, block_size, total_size):
        nonlocal last_progress
        downloaded = block_num * block_size
        downloaded_mb = downloaded / 1024 / 1024
        total_size_mb = total_size / 1024 / 1024
        progress = int((downloaded / total_size) * 100) if total_size > 0 else 0
        if progress > last_progress:
            last_progress = progress
            print(f"\rDownload Progress: {progress}% ({downloaded_mb:.2f}/{total_size_mb:.2f} MB)")

    print(f'Downloading "{url}" to "{filename}"')
    urllib.request.urlretrieve(url, filename, reporthook=download_progress_hook)
    print('\n')

    shutil.copy(filename, destination)


def download_and_unzip_file(url: str, extract_dir: str):
    filename = Path('downloaded_file.zip')
    download_file(url, filename)
    extract_dir.mkdir(parents=True, exist_ok=True)

    print(f'Unzipping "{filename}" to "{extract_dir}"')
    with zipfile.ZipFile(filename, 'r') as zip_ref:
        zip_ref.extractall(extract_dir)
