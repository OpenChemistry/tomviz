# Crop the data over the specified range.
#
# Developed as part of the tomviz project (www.tomviz.com)


def transform_scalars(dataset, START_CROP=[0, 0, 0], END_CROP=None):
    from tomviz import utils

    data_py = utils.get_array(dataset) #get data as numpy array

    if data_py is None: #Check if data exists
        raise RuntimeError("No data array found!")

    (Nx, Ny, Nz) = data_py.shape

    # Crop the data.
    data_cropped = data_py[START_CROP[0]:END_CROP[0],
                           START_CROP[1]:END_CROP[1],
                           START_CROP[2]:END_CROP[2]]

    # Set the data so that it is visible in the application.
    utils.set_array(dataset, data_cropped, START_CROP)
