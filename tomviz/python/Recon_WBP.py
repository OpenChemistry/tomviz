def transform_scalars(dataset):
    """3D Reconstruct from a tilt series using Weighted Back-projection Method"""
    
    from tomviz import utils
    import numpy as np
    interpolation_methods = ('linear','nearest','spline','cubic')
    filter_methods = ('none','ramp','shepp-logan','cosine','hamming','hann')

    ###Nrecon###
    ###filter###
    ###interp###
    
    #Get Tilt angles
    tilt_angles = utils.get_tilt_angles(dataset)
    
    data_py = utils.get_array(dataset)
    if data_py is None:
        raise RuntimeError("No scalars found!")

    recon = wbp3(data_py,tilt_angles,Nrecon,filter_methods[filter],interpolation_methods[interp])
    print('Reconsruction Complete')

    # set the result as the new scalars.
    utils.set_array(dataset, recon)
    
    # Mark dataset as volume
    utils.mark_as_volume(dataset)


import numpy as np
from scipy.interpolate import interp1d

def wbp3(input,angles,N=None,filter="ramp",interp="linear"):
    print "you are using 3D back-projection reconstruction method"
    input = np.double(input)
    (Nslice,Nray,Nproj) = input.shape
    if Nproj!=angles.size:
        raise ValueError('Data does not match angles!')
    interpolation_methods = ('linear','nearest','spline','cubic')
    if not interp in interpolation_methods:
        raise ValueError("Unknown interpolation: %s" % interp)
    if not N: #if ouput size is not given
        N = int(np.floor(np.sqrt(Nray**2/2.0)))
    
    recon = np.zeros((Nslice,N,N))
    for i in range(Nslice):
        #print "slice No.", i+1
        recon[i,:,:] = wbp2(input[i,:,:],angles,N,filter,interp)
    return recon

#weighted back projection reconstruction for 2D image
#Inputs:
#sinogram: dimension: 2D array
#angles: in degrees
#N: output size. Default:
#filter: Fourier filter: 
#       "none","ramp","shepp-logan","cosine","hamming","hann". Default: "ramp"
#interp: back projection interpolation method. Default: linear
#       "linear","nearest","spline","cubic". Default: "linear"

def wbp2(sinogram,angles,N=None,filter="ramp",interp="linear"):
    #print "Sinogram size is:", sinogram.shape
    #print "Angles size is:", angles.shape
    #check inputs
    if sinogram.ndim!=2:
        raise ValueError('Sinogram must be 2D')
    (Nray,Nproj) = sinogram.shape
    if Nproj!=angles.size:
        raise ValueError('Sinogram does not match angles!')
 
    interpolation_methods = ('linear','nearest','spline','cubic')
    if not interp in interpolation_methods:
        raise ValueError("Unknown interpolation: %s" % interp)
    if not N: #if ouput size is not given
        N = int(np.floor(np.sqrt(Nray**2/2.0)))
    
    ang = np.double(angles)*np.pi/180.0
    #create Fourier filter
    F = makeFilter(Nray,filter)
    #pad sinogram for filtering
    s = np.lib.pad(sinogram,((0,F.size-Nray),(0,0)),'constant',constant_values=(0,0)) 
    #apply Fourier filter
    s = np.fft.fft(s,axis=0) * F
    s = np.real(np.fft.ifft(s,axis=0))
    #change back to original 
    s = s[:Nray,:]
      
    #Back projection
    recon = np.zeros((N,N))
    center_proj = Nray // 2 #index of center of projection 
    [X, Y] = np.mgrid[0:N,0:N]
    xpr = X - int(N) // 2
    ypr = Y - int(N) // 2

    for j in range(Nproj):
        t = ypr * np.cos(ang[j]) - xpr * np.sin(ang[j])
        x = np.arange(Nray) - center_proj
        if interp == 'linear': 
            bp = np.interp(t,x,s[:,j],left=0,right=0)
        elif interp == 'spline':
            interpolant = interp1d(x,s[:,j],kind='slinear',bounds_error=False,fill_value=0)
            bp = interpolant(t)
        else:
            interpolant = interp1d(x,s[:,j],kind=interp,bounds_error=False,fill_value=0)
            bp = interpolant(t)
        recon = recon + bp
    
    #normalize
    recon = recon *np.pi/2/Nproj
    return recon

#Filter (1D) projections
def makeFilter(Nray,filterMethod="ramp"):
    filter_methods = ('none','ramp','shepp-logan','cosine','hamming','hann')
    #calculate next power of 2 
    N2 = 2**np.ceil(np.log2(Nray))
    #make a ramp filter
    freq = np.fft.fftfreq(int(N2)).reshape(-1,1)
    omega = 2*np.pi*freq
    filter = 2 * np.abs(freq)

    if filterMethod=="ramp":
        pass
    elif filterMethod=="shepp-logan":
        filter[1:] = filter[1:] * np.sin(omega[1:]) / omega[1:]
    elif filterMethod=="cosine":
        filter[1:] = filter[1:] * np.cos(filter[1:])
    elif filterMethod=="hamming":
        filter[1:] = filter[1:] * (0.54 + 0.46*np.cos(omega[1:]/2) )
    elif filterMethod=="hann":
        filter[1:] = filter[1:] * (1 + np.cos(omega[1:]/2) ) / 2
    elif filterMethod=="none":
        filter[:] = 1
    else:
        raise ValueError("Unknown filter: %s" % filterMethod)

    return filter
    


