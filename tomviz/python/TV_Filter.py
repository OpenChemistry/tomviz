import numpy as np
from numpy.fft import fftn, fftshift, ifftn, ifftshift
import tomviz.operators


class ArtifactsTVOperator(tomviz.operators.CancelableOperator):

    def transform(self, dataset, Niter=100, a=0.1, wedgeSize=5, kmin=5,
                  theta=0):
        """
        Remove Structured Artifacts with Total Variation Minimization"""

        #Import information from dataset
        array = dataset.active_scalars
        (nx, ny, nz) = array.shape

        # Convert angle from Degrees to Radians.
        theta = (theta+90)*(np.pi/180)
        dtheta = wedgeSize*(np.pi/180)

        #Create coordinate grid in polar
        x = np.arange(-nx/2, nx/2-1, dtype=np.float64)
        y = np.arange(-ny/2, ny/2-1, dtype=np.float64)
        [x, y] = np.meshgrid(x, y, indexing='ij')
        rr = (np.square(x) + np.square(y))
        phi = np.arctan2(x, y)

        #Create the Angular Mask
        mask = np.ones((nx, ny), dtype=np.int8)
        mask[np.where((phi >= (theta-dtheta/2)) &
                      (phi <= (theta+dtheta/2)))] = 0
        mask[np.where((phi >= (np.pi+theta-dtheta/2)) &
                      (phi <= (np.pi+theta+dtheta/2)))] = 0
        mask[np.where((phi >= (-np.pi+theta-dtheta/2)) &
                      (phi <= (-np.pi+theta+dtheta/2)))] = 0
        mask[np.where(rr < np.square(kmin))] = 1 # Keep values below rmin.
        mask = np.array(mask, dtype=bool)

        # Initialize Progress bar.
        self.progress.maximum = nz * Niter

        #Main Loop
        for i in range(nz):

            #FFT of the Original Image.
            FFT_image = fftshift(fftn(array[:, :, i]))

            # Reconstruction starts as random image.
            recon_init = np.random.rand(nx, ny)

            self.progress.message = 'Processing Image No.%d/%d ' % (i+1, nz)

            #TV Artifact Removal Loop
            for j in range(Niter):

                # FFT of Reconstructed Image.
                FFT_recon = fftshift(fftn(recon_init))

                # Impose the Data Constraint
                FFT_recon[mask] = FFT_image[mask]

                #Inverse FFT
                recon_constraint = np.real(ifftn(ifftshift(FFT_recon)))

                # Positivity Constraint
                recon_constraint[recon_constraint < 0] = 0

                # TV Minimization Loop
                recon_minTV = recon_constraint
                d = np.linalg.norm(recon_minTV - recon_init)
                for k in range(20):
                    vst = TVDerivative(recon_minTV, nx, ny)
                    recon_minTV = recon_minTV - a*d*vst

                    if self.canceled:
                        return

                # Initializte the Next Loop.
                recon_init = recon_minTV

                # Update the Progress Bar.
                self.progress.value = i*Niter + j

            # Return reconstruction into stack.
            array[:, :, i] = recon_constraint

        #Set the result as the new scalars.
        dataset.active_scalars = np.asfortranarray(array)


def TVDerivative(img, nx, ny):
    fxy = np.pad(img, (1, 1), 'constant', constant_values=0)
    fxnegy = np.roll(fxy, -1, axis=0) #up
    fxposy = np.roll(fxy, 1, axis=0)  #down
    fnegxy = np.roll(fxy, -1, axis=1) #left
    fposxy = np.roll(fxy, 1, axis=1)  #right
    fposxnegy = np.roll(np.roll(fxy, 1, axis=1), -1, axis=0) #right and up
    fnegxposy = np.roll(np.roll(fxy, -1, axis=1), 1, axis=0) #left and down
    vst1 = (2*(fxy - fnegxy) +
            2*(fxy - fxnegy))/(np.sqrt(1e-8 +
                                       (fxy - fnegxy)**2 + (fxy - fxnegy)**2))
    vst2 = (2*(fposxy -
               fxy))/np.sqrt(1e-8 +
                             (fposxy - fxy)**2 + (fposxy - fposxnegy)**2)
    vst3 = (2*(fxposy -
               fxy))/np.sqrt(1e-8 +
                             (fxposy - fxy)**2 + (fxposy - fnegxposy)**2)
    vst = vst1 - vst2 - vst3
    vst = vst[1:-1, 1:-1]
    vst = vst/np.linalg.norm(vst)
    return vst
