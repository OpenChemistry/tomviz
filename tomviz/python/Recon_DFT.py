#3D Reconstruct from a tilt series using Direct Fourier Method
#
#NOTE: Tilt Axis needs to be in the third dimension
#
#developed as part of the tomviz project (www.tomviz.com)

def transform_scalars(dataset):
    from tomviz import utils
    import numpy as np

    #----USER SPECIFIED VARIABLES-----#
    TILT_AXIS  = []  #Specify the tilt axis, if none is specified
                     #it assume it is the smallest dimension
    ANGLES = []  #Specify the angles used in the tiltseries 
                 #For example, ANGLES = np.linspace(0,140,70)
    #---------------------------------#

    data_py = utils.get_array(dataset)
    if data_py is None:
        raise RuntimeError("No scalars found!")
        
    if TILT_AXIS == []: #If tilt axis is not given, find it
    #Find smallest array dimension, assume it is the tilt angle axis
        if data_py.ndim == 3:
            TILT_AXIS = np.argmin( data_py.shape )
        elif data_py.ndim == 2:
            raise RuntimeError("Data Array is 2 dimensions, it should be 3!")
        else: 
            raise RuntimeError("Data Array is not 2 or 3 dimensions!")
    
    if ANGLES == []: #If angles are not given, assume 2 degree tilts
        angles = np.linspace(0,146,74)
        
    dimx = np.size(data_py,0) #IN THE FUTURE THIS SHOULD NOT BE ASSUMED
    result3D = dfm3(data_py,angles,dimx)
    
    # set the result as the new scalars.
    utils.set_array(dataset, result3D)
    print('Reconsruction Complete')

#dfm3(input,angles,Npad)
#3D Direct Fourier Method
#Use numpy fft
#
#Yi Jiang
#08/25/2014
#input: aligned data
#angles: projection angles
#N_pad: size of padded projection. typical choice: twice as the original projection

import numpy as np

def dfm3(input,angles,Npad):
    print "You are using 3D direct fourier method, be patient."
    input = np.double(input)
    (Nx,Ny,Np) = input.shape
    angles = np.double(angles)
    cen = np.floor(Ny/2)
    cen_pad = np.floor(Npad/2)
    pad_post = np.ceil((Npad-Ny)/2); pad_pre = np.floor((Npad-Ny)/2)    
    #initialize value and weight matrix
    print "initialization"
    Nz = Ny
    w = np.zeros((Nx,Ny,Nz))
    v = np.zeros((Nx,Ny,Nz)) + 1j*np.zeros((Nx,Ny,Nz))

    dk = np.double(Ny)/np.double(Npad) *1
    
    for a in range(0,Np):
        print angles[a]
        ang = angles[a]*np.pi/180
	projection = input[:,:,a] #projection
        p = np.lib.pad(projection,((0,0),(pad_pre,pad_post)),'constant',constant_values=(0,0)) #pad zeros
        p = np.fft.ifftshift(p) 
	pF = np.fft.fft2(p)
	pF = np.fft.fftshift(pF)
        
        #bilinear extrapolation
        for i in range(0, np.int(Npad)):
            ky = (i-cen_pad)*dk;  #kz = 0;
            ky_new = np.cos(ang)*ky #new coord. after rotation
            kz_new = np.sin(ang)*ky 
	    sy = abs(np.floor(ky_new) - ky_new) #calculate weights
            sz = abs(np.floor(kz_new) - kz_new)
            for b in range(1,5): #bilinear extrapolation
                pz,py,weight = bilinear(kz_new,ky_new,sz,sy,Ny,b)
		pz = pz + cen;
		py = py + cen;
                if (py>=0 and py<Ny and pz>=0 and pz<Nz):
                   w[:,py,pz] = w[:,py,pz] + weight
	           v[:,py,pz] = v[:,py,pz] + weight * pF[:,i]
    v[w!=0] = v[w!=0]/w[w!=0]
    v = np.fft.ifftshift(v)
    recon = np.real(np.fft.ifftn(v))
    recon = np.fft.fftshift(recon)
    return recon.astype(np.float32)


def bilinear(kz_new,ky_new,sz,sy,N,p):
    if p==1:
       py = np.floor(ky_new)
       pz = np.floor(kz_new)
       weight = (1-sy)*(1-sz)
    elif p==2:
       py = np.ceil(ky_new) #P2
       pz = np.floor(kz_new)
       weight = sy*(1-sz)
    elif p==3:
       py = np.floor(ky_new) #P3
       pz = np.ceil(kz_new)
       weight = (1-sy)*sz
    elif p==4:
       py = np.ceil(ky_new) #P4
       pz = np.ceil(kz_new)
       weight = sy*sz
    if py<0:
       py = N+py
    else:
       py = py
    return (pz,py,weight)

    
