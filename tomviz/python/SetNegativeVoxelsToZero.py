def transform_scalars(dataset):
    """Set negative voxels to zero"""
    
    from tomviz import utils
    import numpy as np

    data = utils.get_array(dataset)

    data[data<0] = 0 #set negative voxels to zero
  
    # set the result as the new scalars.
    utils.set_array(dataset, data)
    