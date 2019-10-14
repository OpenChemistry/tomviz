def transform(dataset, SX=0.5, SY=0.5, SZ=0.5, noise=15.0):
    """Deblur Images with a Weiner Filter."""

    import numpy as np
    from numpy import exp, square, pi
    from scipy.fftpack import fftn, ifftn, fftshift

    #Import information from dataset
    array = dataset.active_scalars
    dim = array.shape

    #Point Spread Function (Estimated with Gaussian Function)
    def gaussian(SX, SY, SZ):
        x = np.arange(-dim[0]/2, dim[0]/2)
        y = np.arange(-dim[1]/2, dim[1]/2)
        z = np.arange(-dim[2]/2, dim[2]/2)
        [x, y, z] = np.meshgrid(x, y, z, indexing='ij')
        Kx = x/dim[0]
        Ky = y/dim[1]
        Kz = z/dim[2]
        r = square(Kx)+square(Ky)+square(Kz)
        G = exp(-2*square(pi)*SX*SY*SZ*r)

        return G

    #Wiener Transformation/Reconstruction
    def wiener(psf, img, SZ):
        snr = noise
        if snr != 0:
            W = (1/psf)*(square(abs(psf))/(square(abs(psf))+1/snr))
        else:
            W = (1/psf)

        img = abs(ifftn(fftshift(W*(fftn(img)))))
        img = img.real.astype(float, order='F')
        return img

    #Transformation/Reconstruction
    H = gaussian(SX, SY, SZ)
    H = H + np.finfo(complex).tiny
    final = wiener(H, array, SZ)
    final = final/np.amax(final) #Normalize the Image

    #Set the result as the new scalars.
    dataset.active_scalars = np.asfortranarray(final)
