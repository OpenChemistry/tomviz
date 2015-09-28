def generate_dataset(array):
    import numpy as np

    #------USER SPECIFIED VARIABLES-----#
    ###p_in### #internal structure factor.
    ###p_s### #shape factor.
    ###sparsity###  #sparsity: percentage of non-zero pixels. Range:[0,1]

    #-----------------------------------#

    arrayShape = array.shape
    x = np.fft.fftfreq(arrayShape[0])
    y = np.fft.fftfreq(arrayShape[1])
    z = np.fft.fftfreq(arrayShape[2])
    
    X,Y,Z = np.meshgrid(y,x,z)
    kr = np.sqrt(X**2 + Y**2 + Z**2)

    #Create inner structure
    A = np.exp(-p_in*kr) #generate amplitude
    phase = np.random.randn(arrayShape[0],arrayShape[1],arrayShape[2]) #generate phase
    F = A*np.exp(2*np.pi*1j*phase) #combine amplitude and phase
    f = np.fft.ifftn(F) #inverse FFT
    f_in = np.absolute(f).copy().flatten() #

    #Create shape
    A = np.exp(-p_s*kr) #generate amplitude
    phase = np.random.randn(arrayShape[0],arrayShape[1],arrayShape[2]) #generate phase
    F = A*np.exp(2*np.pi*1j*phase)#combine amplitude and phase
    f = np.fft.ifftn(F) #inverse FFT
    f_shape = np.absolute(f)

    #Impose sparsity (% of non-zero voxels)
    f_shape = np.argsort(f_shape,axis=None) #sort the shape image
    f_shape = f_shape.flatten()
    N_zero = np.int(np.round((array.size*(1-sparsity)))) #number of zero voxels
    f_shape[N_zero:]=f_shape[N_zero]
    f_in[f_shape]=0

    f_in = f_in/np.amax(f_in) #Normalize image
    np.copyto(array,f_in.reshape(arrayShape)) #update array

