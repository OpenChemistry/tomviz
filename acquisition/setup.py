from setuptools import setup, find_packages

dm3_url = 'git+https://cjh1@bitbucket.org/cjh1/pydm3reader.git' \
    '@filelike#egg=dm3_lib-1.2'
bottle_url = 'https://github.com/bottlepy/bottle/archive/41ed6965.zip' \
    '#egg=bottle-0.13-dev'

setup(
    name='tomviz-acquisition',
    version='0.0.1',
    description='Tomviz acquisition server.',
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
    extras_require={
        'tiff': ['Pillow'],
        'test': ['requests', 'Pillow', 'mock', 'diskcache']
    },
    entry_points={
        'console_scripts': [
            'tomviz-acquisition = tomviz_acquisition.acquisition.cli:main',
            'tomviz-tiltseries-writer = tests.mock.tiltseries.writer:main'
        ]
    }
)
