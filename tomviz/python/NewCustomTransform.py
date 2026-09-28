import tomviz.nodes


class NewCustomTransform(tomviz.nodes.TransformNode):
    """Starting point for a custom transform.

    The JSON description beside this script declares the ports and the
    parameters. Each input arrives in the ``inputs`` dictionary under its
    declared name, each parameter as a keyword argument of transform(),
    and the returned dictionary provides the declared outputs.
    """

    def transform(self, inputs):
        import numpy as np

        dataset = inputs["volume"]

        # Operate on every scalar array of the input; here we square root.
        output = dataset.apply_to_each_scalar_array(
            lambda scalars: np.sqrt(np.float32(scalars)))

        return {"volume": output}
