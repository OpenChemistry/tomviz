def transform_scalars(dataset):
    """Define this method for Python operators that 
    transform input scalars"""
    
    from tomviz import utils
    import numpy as np

    scalars = utils.get_scalars(dataset)
    if scalars is None:
        raise RuntimeError("No scalars found!")

    if scalars.min() < 0:
        print("WARNING: Square root of negative values results in NaN!)

    # transform the dataset
    result = np.sqrt(scalars)
    
    # set the result as the new scalars.
    utils.set_scalars(dataset, result)