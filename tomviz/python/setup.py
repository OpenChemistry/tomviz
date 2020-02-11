from setuptools import setup, find_packages

setup(
    name='tomviz-pipeline',
    version='0.0.1',
    description='Tomviz python external pipeline execution infrastructure.',
    author='Kitware, Inc.',
    author_email='kitware@kitware.com',
    url='https://www.tomviz.org/',
    license='BSD 3-Clause',
    classifiers=[
        'Development Status :: 3 - Alpha',
        'License :: OSI Approved :: BSD 3-Clause',
        'Operating System :: OS Independent',
        'Programming Language :: Python',
        'Programming Language :: Python :: 2',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.5'
    ],
    packages=find_packages(),
    install_requires=['tqdm', 'h5py', 'numpy==1.16.4', 'click', 'scipy',
                      'itk', 'pyfftw'],
    extras_require={
        'interactive': ['jsonpatch', 'marshmallow']
    },
    entry_points={
        'console_scripts': [
            'tomviz-pipeline = tomviz.cli:main'
        ]
    }
)
