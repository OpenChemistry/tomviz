from pathlib import Path
import os
import sys

from .fix_pdb import fix_pdb

fix_pdb()


# Override sys.executable to contain an actual Python executable, if available
if not Path(sys.executable).exists():
    if 'CONDA_PREFIX' in os.environ:
        try_path = Path(os.environ['CONDA_PREFIX']) / 'bin/python'
        if try_path.exists():
            sys.executable = str(try_path)
