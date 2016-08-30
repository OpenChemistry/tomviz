clang-format
------------

We use [clang-format](http://llvm.org/releases/3.8.0/tools/clang/docs/ClangFormatStyleOptions.html)
to keep formatting in the code base consistent. Please run clang-format
on your patches before submitting.

clang-format ships with a python script ```clang/tools/clang-format-diff.py``
that can be used to reformat patches. For example the following command will
reformat all the lines in the latest commit

```shell
git diff -U0 HEAD^ | clang-format-diff.py -i -p1

```

Code style
----------

This project is developed primarily in C++ and Python. Please follow these
code style guidelines when contributing code to our project.

* Alphabetize includes

* Use #include "xxx" for local includes, #include <xxx> for external
  includes.

* Do not add comment separators before function definitions.

* Split long when reasonable lines to avoid going over 80 characters per line.

* Add a space after the commas in parameter lists, e.g.,
  function(a, b, c), not function(a,b,c)

* Add spaces between operators, e.g. 5 - 2, not 5-2.

* For local variables, use camelCase, starting variable names with a
  lower-case letter.

* For comments, add a space between // and the beginning of the
  comment, e.g.,

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

* Assume that C++11 features are available, and prefer them over legacy
  macros, defines, etc. A few examples follow, but are not exhaustive.

    * Use override to specify member overrides in derived classes.
    * Set default values of member variables directly in definitions.
    * Use nullptr instead of NULL.

* When creating VTK subclasses please follow the tomviz style outlined here.
