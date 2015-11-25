def transform_scalars(dataset):
    """3D Reconstruct from a tilt series using Direct Fourier Method"""
    
    from tomviz import utils
    import numpy as np
    
    #Get Tilt angles
    tilt_angles = utils.get_tilt_angles(dataset)
    
    data_py = utils.get_array(dataset)
    if data_py is None:
        raise RuntimeError("No scalars found!")

    recon = dfm3(data_py,tilt_angles,np.size(data_py,0)*2)
    print('Reconsruction Complete')

    # set the result as the new scalars.
    utils.set_array(dataset, recon)

    # Mark dataset as volume
    utils.mark_as_volume(dataset)

import numpy as np
def dfm3(input,angles,Npad):
    #input: aligned data
    #angles: projection angles
    #N_pad: size of padded projection. Typical choice: twice as the original projection

    input = np.double(input)
    (Nx,Ny,Nproj) = input.shape
    angles = np.double(angles)
    cen = np.floor(Ny/2)
    cen_pad = np.floor(Npad/2)
    pad_post = np.ceil((Npad-Ny)/2); pad_pre = np.floor((Npad-Ny)/2)    
    #initialize value and weight matrix
    Nz = Ny
    w = np.zeros((Nx,Ny,Nz))
    v = np.zeros((Nx,Ny,Nz)) + 1j*np.zeros((Nx,Ny,Nz))

    dk = np.double(Ny)/np.double(Npad) *1
    
    for a in range(0,Nproj):
        #print angles[a]
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

#bilinear extrapolation
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

    
