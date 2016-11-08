def transform_scalars(dataset):
    """Automatically align tilt images by cross-correlation"""

    from tomviz import utils
    import numpy as np

    tiltSeries = utils.get_array(dataset).astype(float)
    tiltAngles = utils.get_tilt_angles(dataset)

    #determine reference image
    zeroDegreeTiltImage = np.where(tiltAngles == 0)[0]
    if zeroDegreeTiltImage:
        referenceIndex = zeroDegreeTiltImage[0]
    else:
        referenceIndex = tiltSeries.shape[2] // 2
    referenceImage = tiltSeries[:,:,referenceIndex]

    #create Fourier space filter
    filterCutoff = 4
    (Ny, Nx) = referenceImage.shape
    ky = np.fft.fftfreq(Ny)
    kx = np.fft.fftfreq(Nx)
    [kX,kY] = np.meshgrid(kx,ky)
    kR = np.sqrt(kX**2 + kY**2)
    kFilter = (kR <= (0.5/filterCutoff)) * np.sin(2*filterCutoff*np.pi*kR)**2

    #create real sapce filter to remove edge discontinuities
    y = np.linspace(1,Ny,Ny)
    x = np.linspace(1,Nx,Nx)
    [X,Y] = np.meshgrid(x,y)
    rFilter = (np.sin(np.pi*X/Nx) *np.sin(np.pi*Y/Ny)) ** 2

    referenceImage = (referenceImage - np.mean(referenceImage)) * rFilter
    referenceImageF = np.fft.fft2(referenceImage)
    for i in range(tiltSeries.shape[2]):
        if i != referenceIndex:
            currentImage = tiltSeries[:, :, i ]
            currentImage = (currentImage - np.mean(currentImage ))* rFilter
            currentImageF = np.fft.fft2(currentImage)
            xcor = np.fft.fftshift(abs(np.fft.ifft2(np.conj(currentImageF)*referenceImageF*kFilter)))
            shifts = np.unravel_index(xcor.argmax(), xcor.shape)
            print shifts
            tiltSeries[:, :, i] = np.roll(tiltSeries[:, :, i], -shifts[0], axis=0)
            tiltSeries[:, :, i] = np.roll(tiltSeries[:, :, i], -shifts[1], axis=1)

    utils.set_array(dataset, tiltSeries)
    print('Align Images Complete')
