# Crop the data over the specified range.
#
# Developed as part of the tomviz project (www.tomviz.com)

def transform_scalars(dataset):
    from tomviz import utils
    import numpy as np
    
    data_py = utils.get_array(dataset) #get data as numpy array
    
    if data_py is None: #Check if data exists
        raise RuntimeError("No data array found!")    
    
    (Nx, Ny, Nz) = data_py.shape
    
    #----USER SPECIFIED VARIABLES-----#
    #START_CROP= [0,0,0]        # Specify start and end point of crop cube
    #END_CROP  = [Nx-1,Ny-1,Nz-1] # <-- Default will keep all data in range
    ###START_CROP###
    ###END_CROP###
    #---------------------------------#    

    # Crop the data.
    data_cropped = data_py[START_CROP[0]:END_CROP[0],START_CROP[1]:END_CROP[1],START_CROP[2]:END_CROP[2]]
    
    # Set the data so that it is visible in the application.
    utils.set_array(dataset, data_cropped)
    print('Data has been cropped.')
