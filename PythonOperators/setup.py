from distutils.core import setup
from Cython.Build import cythonize

setup(
    ext_modules = cythonize("_NLM_Filter.pyx", annotate=True)
)