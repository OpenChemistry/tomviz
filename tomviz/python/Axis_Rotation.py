#Find in-plane rotation of tomography tilt series from FFT variance
#
#
#developed as part of the tomviz project (www.tomviz.com)

def transform_scalars(dataset):

    #----USER SPECIFIED VARIABLES-----#
    STACK_DIM  = []  #Specify image stack dimension x=0, y=1, z=2
    #---------------------------------#
        
    from tomviz import utils
    import numpy as np
    
    data_py = utils.get_array(dataset) #get data as numpy array
    
    if data_py is None: #Check if data exists
        raise RuntimeError("No data array found!")    
    
    if STACK_DIM == []: #If stack dimension is not given, find it
    #Find smallest array dimension, assume it is the stack dimension
        if data_py.ndim == 3:
            STACK_DIM = np.argmin( data_py.shape )
        elif data_py.ndim == 2:
            raise RuntimeError("Data Array is 2 dimensions, it should be 3!")
        else: 
            raise RuntimeError("Data Array is not 2 or 3 dimensions!")
    
    print('Finding in-plane rotation: assuming images are stacked in dimension 3.')
    Intensity = np.zeros(data_py.shape)
    #take FFT of all projections
    for i in range(0,data_py.shape[2]):
        p = data_py[:,:,i]
        pF = np.absolute(np.fft.fft2(data_py[:,:,i]))
        if (i==0):
           a = pF[0,0]
        Intensity[:,:,i] = np.fft.fftshift(pF/a)
    #rescale intensity
    Intensity = np.power(Intensity,0.2)
    #calculate variation itensity image
    Intensity_var = np.var(Intensity,axis=2)

    #start with coarse search
    coarse_step=2
    angles = np.arange(-90,90,coarse_step)
    rot_ang=find_min_line(Intensity_var,angles)

    #now refine
    fine_step=0.1
    angles = np.arange(rot_ang-coarse_step,rot_ang+coarse_step,fine_step)
    rot_ang=find_min_line(Intensity_var,angles)

    print('Align Images Complete')
    print('Found in-plane rotation %.1f' % rot_ang)



def find_min_line(Intensity_var,angles):
    import numpy as np

    Nx = Intensity_var.shape[0]; Ny = Intensity_var.shape[1]
    cenx = np.floor(Nx/2); ceny = np.floor(Ny/2)

    N = np.round(Nx/3)
    cen = np.floor(N/2)
    Intensity_line = np.zeros((angles.size,N))
    w = np.zeros(N)
    v = np.zeros(N)

    for a in range(0,angles.size):
        ang = angles[a]*np.pi/180
        w = np.zeros(N)
        v = np.zeros(N)
        for i in range(0,N):
            x = -i*np.sin(ang); y = i*np.cos(ang);
            sx = abs(np.floor(x) - x); sy = abs(np.floor(y) - y)
            px = np.floor(x)+cenx; py = np.floor(y)+ceny
            if (px>=0 and px<Nx and py>=0 and py<Ny):
               w[i] = w[i]+ (1-sx)*(1-sy)
               v[i] = v[i]+ (1-sx)*(1-sy)*Intensity_var[px,py]
            px = np.ceil(x)+cenx; py = np.floor(y)+ceny
            if (px>=0 and px<Nx and py>=0 and py<Ny):
               w[i] = w[i]+ sx*(1-sy)
               v[i] = v[i]+ sx*(1-sy)*Intensity_var[px,py]
            px = np.floor(x)+cenx; py = np.ceil(y)+ceny
            if (px>=0 and px<Nx and py>=0 and py<Ny):
               w[i] = w[i]+ (1-sx)*sy
               v[i] = v[i]+ (1-sx)*sy*Intensity_var[px,py]
            px = np.ceil(x)+cenx; py = np.ceil(y)+ceny
            if (px>=0 and px<Nx and py>=0 and py<Ny):
               w[i] = w[i]+ sx*sy
               v[i] = v[i]+ sx*sy*Intensity_var[px,py]
        v[w!=0] = v[w!=0]/w[w!=0]
        Intensity_line[a,:] = v
    Intensity_line_sum = np.sum(Intensity_line,axis=1)
    rot_ang = angles[np.argmin(Intensity_line_sum)]

    return rot_ang
