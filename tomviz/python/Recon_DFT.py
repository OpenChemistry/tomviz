def transform_scalars(dataset):
    """3D Reconstruct from a tilt series using Direct Fourier Method"""

    from tomviz import utils
    import numpy as np

    # Get Tilt angles
    tilt_angles = utils.get_tilt_angles(dataset)

    data_py = utils.get_array(dataset)
    if data_py is None:
        raise RuntimeError("No scalars found!")

    recon = dfm3(data_py,tilt_angles,np.size(data_py,0)*2)
    print('Reconsruction Complete')

    # Set the result as the new scalars.
    utils.set_array(dataset, recon)

    # Mark dataset as volume
    utils.mark_as_volume(dataset)

import pyfftw
import numpy as np

def dfm3(input,angles,Npad):
    # input: aligned data
    # angles: projection angles
    # N_pad: size of padded projection.

    input = np.double(input)
    (Nx,Ny,Nproj) = input.shape
    angles = np.double(angles)
    cen = np.floor(Ny/2)
    cen_pad = np.floor(Npad/2)
    pad_post = np.ceil((Npad-Ny)/2); pad_pre = np.floor((Npad-Ny)/2)
    
    # Initialization
    Nz = Ny
    w = np.zeros((Nx,Ny,np.int(Nz/2+1))) #store weighting factors
    v = pyfftw.n_byte_align_empty((Nx,Ny,Nz/2+1),16,dtype='complex128')
    v = np.zeros(v.shape) + 1j*np.zeros(v.shape)
    recon = pyfftw.n_byte_align_empty((Nx,Ny,Nz),16,dtype='float64')
    recon_fftw_object = pyfftw.FFTW(v,recon,direction='FFTW_BACKWARD',axes=(0,1,2))

    p = pyfftw.n_byte_align_empty((Nx,Npad),16,dtype='float64')
    pF = pyfftw.n_byte_align_empty((Nx,Npad/2+1),16,dtype='complex128')
    p_fftw_object = pyfftw.FFTW(p,pF,axes=(0,1))

    dk = np.double(Ny)/np.double(Npad)

    for a in range(0, Nproj):
        print angles[a]
        ang = angles[a]*np.pi/180
        projection = input[:,:,a] #2D projection image
        p = np.lib.pad(projection,((0,0),(pad_pre,pad_post)),'constant',constant_values=(0,0)) #pad zeros
        p = np.fft.ifftshift(p) 
        p_fftw_object.update_arrays(p,pF)
        p_fftw_object()

        if ang<0:
           pF = np.conj(pF)
           ang = np.pi+ang
        
        # Bilinear extrapolation
        for i in range(0,np.int(np.ceil(Npad/2))+1):
            ky = i*dk;  #kz = 0;
            ky_new = np.cos(ang)*ky #new coord. after rotation
            kz_new = np.sin(ang)*ky 
            sy = abs(np.floor(ky_new) - ky_new) #calculate weights
            sz = abs(np.floor(kz_new) - kz_new)
            
            for b in range(1,5): #bilinear extrapolation
                pz,py,weight = bilinear(kz_new,ky_new,sz,sy,Ny,b)
                if (py>=0 and py<Ny and pz>=0 and pz<Nz/2+1):
                    w[:,py,pz] = w[:,py,pz] + weight
                    v[:,py,pz] = v[:,py,pz] + weight * pF[:,i]

    v[w!=0] = v[w!=0]/w[w!=0]
    recon_F = v.copy()
    recon_fftw_object.update_arrays(v,recon)
    recon_fftw_object()
    recon = np.fft.fftshift(recon)
    return recon.astype(np.float32)

# Bilinear extrapolation
def bilinear(kz_new,ky_new,sz,sy,N,p):
    if p==1:
        py = np.floor(ky_new)
        pz = np.floor(kz_new)
        weight = (1-sy)*(1-sz)
    elif p==2:
        py = np.ceil(ky_new)
        pz = np.floor(kz_new)
        weight = sy*(1-sz)
    elif p==3:
        py = np.floor(ky_new)
        pz = np.ceil(kz_new)
        weight = (1-sy)*sz
    elif p==4:
        py = np.ceil(ky_new)
        pz = np.ceil(kz_new)
        weight = sy*sz
    if py<0:
        py = N+py
    else:
        py = py
    return (pz,py,weight)
