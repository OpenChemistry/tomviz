Implementing Python operators
=============================

There are a couple of ways to implement a Python operator.

Writing a transform_scalars function
-----------------------------------
The simplest operator implementation is to provide a function called ```transform_scalars```
in a module.

```python

def transform_scalars(self):
    # Do work here

```

Subclassing tomviz.operators.Operator
-------------------------------------

Tomviz provides an operator base class that can be used to implement a Python operator. To
create an operator simply subclass and provide an implementation of the
'transform_scalars' method.

```python

import tomviz.operators

class MyOperator(tomviz.operators.Operator):
    def transform_scalars(self, data):
        # Do work here

```

Subclassing tomviz.operators.CancelableOperator
-----------------------------------------------

To implement an operator that can be canceled the operator should be derived
from ```tomviz.operators.CancelableOperator```. This provides an additional
property called ```canceled``` that can be used to determine if the operator
execution have been canceled by the user. The data should be processed in chunks
so that this property can be periodically checked to break out of the execution
if necessary.

```python

import tomviz.operators

class MyCancelableOperator(tomviz.operators.CancelableOperator):
    def transform_scalars(self, data):
         while(not self.canceled):
            # Do work here

```

Operator progress
-----------------

Instances of ```tomviz.operators.Operator``` have a ```progress``` attribute that can be used to
report the progress of an operator. The maximum number of steps the operator will report is held
in the ```progress.maximum``` property and the current progress can be updated using ```progress.value = currrent_value```. A status message can also be set on the progress object to give further feedback to the user ```progress.message = msg```



```python

import tomviz.operators

class MyProgressOperator(tomviz.operators.Operator):
    current_progress = 0
    def transform_scalars(self, data):
        self.progress.maximum = 100
        # Do work here
        current_progress += 1
        self.progress.value = current_progress
```

Generating the user interface automatically
-------------------------------------------

Python operators can take zero or more parameters that govern their operation.
Initializing these parameter values is typically done through a dialog box
presented prior to running the operator. The simplest way to define the user
interface is to describe the parameters in a JSON file that accompanies the
Python script.

The JSON file consists of a few key/value pairs at the top level of the JSON tree:

* `name` - The name of the operator. The name should not contain spaces.
* `label` - The displayed name of the operator as it should appear in the user
interface. No restrictions.
* `description` - Text that describes what the operator does. No restrictions.
* `parameters` - A JSON array of parameters.

An item in the `parameter` array is itself a JSON object consisting of several
name/value pairs.

* `name` - The name of the parameter. This must be a valid Python variable name.
* `label` - The displayed name of the parameter in the user interface. No
restrictions.
* `type` - Parameter type. Currently supported types are:
    * `bool` - Boolean type. Valid values are `true` or `false`.
    * `int` - Integral type. Valid values are in the range of a C integer.
    * `double` - Floating-point type. Valid values are in the range of a C double.
    * `enumeration` - Provides a set of options. Possible values are listed in
    an `options` key/value pair (described below)
    * `xyz_header` - Special type used as a hint for the UI to add the headers
    "X", "Y", and "Z" above columns for 3-element parameters representing
    coordinates.
* `default` - Default value for the parameter. Must be a number or boolean JSON
value `true` or `false`. The default for a multi-element `int` or `double`
parameter is an array of one or more ints or doubles.
* `minimum` - Sets the minimum value that a parameter may be. An array of
values specifies the component-wise minimum of a multi-element parameter.
* `maximum` - Like the `minimum`, but sets the maximum value that a parameter
may be.
* `precision` - Optional number of digits past the decimal for `double`
parameters.
* `options` - An array of JSON objects, each of which contains a single key/value
pair where the key is the name of the option and the value is an integer index
of the options.

Examples of parameter descriptions:

`bool`

```
{
  "name" : "enable_valley_emphasis",
  "label" : "Enable Valley Emphasis",
  "type" : "bool",
  "default" : false
}
```

`int`
```
{
  "name" : "iterations",
  "label" : "Number of Iterations",
  "type" : "int",
  "default" : 100,
  "minimum" : 0
}
```

Multi-element `int`
```
{
  "name" : "shift",
  "label" : "Shift",
  "description" : "The shift to apply",
  "type" : "int",
  "default" : [0, 0, 0]
}
```

`double`
```
{
  "name" : "rotation_angle",
  "label" : "Angle",
  "description" : "Rotation angle in degrees.",
  "type" : "double",
  "default" : 90.0,
  "minimum" : -360.0,
  "maximum" : 360.0,
  "precision" : 1
}
```

Multi-element `double`
```
{
  "name" : "resampling_factor",
  "label" : "Resampling Factor",
  "type" : "double",
  "default" : [1, 1, 1]
}
```

`enumeration`
```
{
  "name" : "rotation_axis",
  "label" : "Axis",
  "description" : "Axis of rotation.",
  "type" : "enumeration",
  "default" : 0,
  "options" : [
    {"X" : 0},
    {"Y" : 1},
    {"Z" : 2}
  ]
}
```

Defining Operator Results and Child Data Sets
---------------------------------------------

In addition to transforming the current data set, operators may produce
additional data sets. The additional data sets are described in the top-level
JSON with the following keys:

* `results` - An array of JSON objects describing the outputs produced by the
operator. Results are additional data objects produced when the operator is run.
Result JSON objects have three key/value pairs:
    * `name` - The name of the result
    * `label` - The displayed name of the result in the UI.
More than one result may be produced by the operator.
* `children` - An array of JSON objects describing child data sets produced by
the operator. Child data sets are similar to results, but are special in that they
must be image data to which additional operators may be applied. A child data set
is described with the same key/value pairs as `results` objects. Currently, only
a single child data set is supported.

The `name` key of each result and child data set must be unique.

Creating Operator Results and Child Data Sets
---------------------------------------------

In the operator Python code, results and child data sets are set in a
dictionary returned by the `transform_scalars` function. This dictionary consists
of key/value pairs where the name is the `name` value of the result or child
data set and the value is the result or child data object. Results and child
data objects are VTK objects created in the Python operator code. See
`ConnectedComponents.py` for an example of how to return both a result and
child data set.
