Implementing Python operators
=============================

There are a couple of ways to implement a Python operator.

Writing a transform_scalar function
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
    def transform_scalar(self, data):
        # Do work here

```

Subclassing tomviz.operators.CancelableOperator
--------------------

To implement an operator that can be canceled the operator should be derived
from ```tomviz.operators.CancelableOperator```. This provides an additional
property called ```canceled``` that can be used to determine if the operator
execution have been canceled by the user. The data should be processed in chunks
so that this property can be periodically checked to break out of the execution
if necessary.

```python

import tomviz.operators

class MyCancelableOperator(tomviz.operators.CancelableOperator):
    def transform_scalar(self, data):
         while(not self.canceled):
            # Do work here

```

