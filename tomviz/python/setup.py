from pathlib import Path

from setuptools import setup, find_packages


def tomviz_pipeline_requirement():
    """The tomviz-pipeline range from tomviz/_pipeline_requirement.py, the
    same one the app's external-environment check enforces. Executed
    rather than imported so setup.py never imports the tomviz package."""
    namespace = {}
    exec((Path(__file__).parent / 'tomviz' /
          '_pipeline_requirement.py').read_text(), namespace)
    return namespace['tomviz_pipeline_requirement']()


setup(
    name='tomviz-app',
    version='0.0.1',
    description='Application-side Python layer for the tomviz desktop app '
                '(VTK-backed datasets, in-app operator runtime, file-format '
                'and beamline integrations).',
    author='Kitware, Inc.',
    author_email='kitware@kitware.com',
    url='https://www.tomviz.org/',
    license='BSD 3-Clause',
    classifiers=[
        'Development Status :: 3 - Alpha',
        'License :: OSI Approved :: BSD 3-Clause',
        'Operating System :: OS Independent',
        'Programming Language :: Python :: 3',
    ],
    python_requires='>=3.9',
    packages=find_packages(),
    install_requires=[tomviz_pipeline_requirement(), 'tqdm', 'h5py',
                      'numpy', 'scipy'],
    extras_require={
        'itk': ['itk'],
        'pyfftw': ['pyfftw']
    },
)
