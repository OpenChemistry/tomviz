Please follow these code style guidelines when developing for tomviz.

* Alphabetize includes

* Use #include "xxx" for local includes, #include <xxx> for external
  includes.

* Do not add comment separators before function definitions as are often
  seen in VTK and ParaView.

* Split long lines to avoid going over 80 characters per line.

* Add a space after the commas in parameter lists, e.g.,
  function(a, b, c), not function(a,b,c)

* Add spaces between operators, e.g. 5 - 2, not 5-2.

* For local variables, use camelCase, starting variable names with a
  lower-case letter.

* For comments, add a space between // and the beginning of the
  comment, e.g.,

    * // A comment
    * \# Python comment

* Curly braces marking the start and end of a code block should be on
  separate lines and aligned vertically with the statement preceding
  the block, e.g.,

        if (condition)
        {
          statement;
        }

        for (int i = 0; i < n; ++i)
        {
          statement;
        }

  Note that this differs from VTK and ParaView's coding style.

* Assume that C++11 features are available.

* Use nullptr instead of NULL.

* When creating VTK subclasses, follow tomviz style instead of VTK style.