from setuptools import setup, find_packages

setup(
    name='tomviz-acquisition-passive',
    version='0.0.1',
    description='Passive acquisition source.',
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
    install_requires=['dm3_lib', 'Pillow'],
    dependency_links=[
        'git+https://cjh1@bitbucket.org/cjh1/pydm3reader.git#filelike'
    ]
)
