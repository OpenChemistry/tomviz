import importlib.util
import inspect
from pathlib import Path
import shutil
from types import ModuleType
import urllib.request
import zipfile

from tomviz.executor import OperatorWrapper
from tomviz.operators import Operator

OPERATOR_PATH = Path(__file__).parent.parent.parent / 'tomviz/python'


def load_operator_module(operator_name: str) -> ModuleType:
    module_path = OPERATOR_PATH / f'{operator_name}.py'
    spec = importlib.util.spec_from_file_location(operator_name, module_path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def load_operator_class(operator_module: ModuleType) -> Operator | None:
    # Locate the operator class
    for v in operator_module.__dict__.values():
        if inspect.isclass(v) and issubclass(v, Operator):
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
