clang-format
------------

We use [clang-format][clang-format] to keep formatting in the code base
consistent. Please run clang-format on your patches before submitting.

clang-format ships with a python script ```clang/tools/clang-format-diff.py``
that can be used to reformat patches. For example the following command will
reformat all the lines in the latest commit

```shell
git diff -U0 HEAD^ | clang-format-diff.py -i -p1

```

clang-format also provides [git-clang-format][git-clang-format], a script that
more closely integrates with git. If you add this script to your path you can
using the following command to reformat all the lines in the latest commit.

```shell
git clang-format HEAD~1

```

flake8
------

We use [flake8][flake8] to test for Python style compliance. The tests are run
when a PR is raised against the repository. You can also run flake8 locally to
reveal problems before you push a branch. First you need to install flake8, the
simplest way todo this is using pip.

```shell
pip install flake8
```

To run flake8 using the project configuration file.

```shell
cd <tomviz_source_dir>
flake8 --config=flake8.cfg .
# Or a specific file
flake8 --config=flake8.cfg tomviz/python/InvertData.py
```

There is also [autopep8](https://pypi.python.org/pypi/autopep8) that can be
using to automatically fix many of the problems raised by flake8. Again is can
be install using pip.

```shell
pip install autopep8
```

To run autopep8 using the project configuration file.

```shell
cd <tomviz_source_dir>
# It is recommended to run it on specfic file(s) you have changed, for example.
autopep8 --ignore E262,E261,E265 --global-config flake8.cfg --in-place tomviz/python/InvertData.py
```

### clang-format-diff locations by platform

The exact location of the Python script varies by platform/distro. The table
below provides the location on some common platform/distro's

| Platform/Distro.  | Location                             |
| ---------------- |:-------------------------------------:|
| Arch Linux       | /usr/share/clang/clang-format-diff.py |
| Ubuntu           | /usr/bin/clang-format-diff-3.8        |

The script can also be downloaded [here][clang-format-diff].

Code style
----------

This project is developed primarily in C++ and Python. Please follow these code
style guidelines when contributing code to our project.

* Alphabetize includes

* Use #include "xxx" for local includes, #include \<xxx\> for external includes.

* Do not add comment separators before function definitions.

* Split long lines, when reasonable, to avoid going over 80 characters per line.

* Add a space after the commas in parameter lists, e.g.,
  function(a, b, c), not function(a,b,c)

* Add spaces between operators, e.g. 5 - 2, not 5-2.

* For class names, use CamelCase, starting their names with an upper-case
  letter.

* For local variables and function names, use camelCase, starting names with a
  lower-case letter.

* For member variables, prefix them with m\_, i.e. m\_camelCase, starting the
  name with a lower-case letter.

* For comments, add a space between // and the beginning of the comment, e.g.,

    * // A comment
    * \# Python comment

* Use 2 spaces when indenting C++ code, 4 spaces for Python code.

* Do not indent inside namespaces, e.g.,

        namespace tomviz {
        void foo();

* Curly braces marking the start and end of a code block should be on
  separate lines and aligned vertically with the statement preceding
  the block, e.g.,

        if (condition) {
          statement;
        }

        for (int i = 0; i < n; ++i) {
          statement;
        }

* Assume that C++11 features are available, and prefer them over legacy macros,
  defines, etc. A few examples follow, but are not exhaustive.

    * Use override to specify member overrides in derived classes.
    * Set default values of member variables directly in definitions.
    * Use nullptr instead of NULL.

* When creating VTK subclasses please follow the Tomviz style outlined here.

[clang-format]: http://llvm.org/releases/3.8.0/tools/clang/docs/ClangFormatStyleOptions.html
[git-clang-format]: https://llvm.org/svn/llvm-project/cfe/trunk/tools/clang-format/git-clang-format
[flake8]: https://pypi.python.org/pypi/flake8
[clang-format-diff]: https://llvm.org/svn/llvm-project/cfe/trunk/tools/clang-format/clang-format-diff.py
