def transform(dataset, clipNum=5):
    """Set values outside a cirular range to minimum(dataset) to
    remove reconstruction artifacts"""

    import numpy as np

    array = dataset.active_scalars

    # Constant values
    numX = array.shape[1]
    numY = array.shape[2]
    minVal = array.min()

    # Find the values to be clipped
    maxR = min([numX, numY]) / 2 - clipNum
    XX, YY = np.mgrid[0:numX, 0:numY]
    RR = np.sqrt((XX - numX / 2) ** 2 + (YY - numY / 2) ** 2)
    clipThese = np.where(RR >= maxR)

    # Apply the mask along the original tilt axis direction
    # (always the first direction in the array)
    for ii in range(0, array.shape[0]):
        curImage = array[ii, :, :] # points to the relevant 2D part of the array
        curImage[clipThese] = minVal

    # Return to tomviz
    dataset.active_scalars = array
