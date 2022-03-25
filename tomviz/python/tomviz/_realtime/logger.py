import tomviz._realtime.wbp as wbp
from tomviz.io import ser
from tomviz.io import dm
import numpy as np
import time
import os


# Class for listening and logging new files for real-time reconstruction
class Logger:

    def __init__(self, listenDirectory, fileExtension, alignMethod, invert):

        # Create Local Directory
        if not os.path.exists(listenDirectory):
            os.makedirs(listenDirectory)

        # Set Member Variables
        self.listenDir = listenDirectory
        self.fileExt = fileExtension
        self.alignMethod = alignMethod
        self.invert = invert

        self.listenFiles = self.listen_files_list(self.listenDir)
        self.logFiles, self.logTiltSeries, self.logTiltAngles = [], [], []
        self.logTiltSeries0 = []
        print("Listener on {} created.".format(self.listenDir))

    def listen_files_list(self, directory):
        """Grab current files in listen directory"""
        files = [f for f in os.listdir(directory) if
                 f[-len(self.fileExt):] == self.fileExt]
        return files

    def CHANGE_appendAll(self):
        """Append stored files for each new element"""
        # Separate new files to be loaded
        FoI = list(set(self.listenFiles)-set(self.logFiles))
        FoI.sort()
        for file in FoI:
            print("Loading {}".format(file))
            filePath = os.path.join(self.listenDir, file)

            try:
                (newProj, newAngle) = self.read_projection_image(filePath)

                self.logTiltAngles = np.append(self.logTiltAngles, newAngle)

                # Invert Contrast for BF-TEM
                if self.invert:
                    newProj *= -1

                newProj = self.background_subtract(newProj)

                # Apply Center of Mass (if selected)
                if self.alignMethod == 'CoM':
                    newProj = self.center_of_mass_align(newProj)

                # Account for Python's disdain for AxAx1 arrays
                # (compresses to 2D)
                if(len(self.logTiltSeries0) == 0):
                    dataDim = np.shape(newProj)
                    self.logTiltSeries0 = np.zeros([dataDim[0], dataDim[1], 1])
                    self.logTiltSeries0[:, :, 0] = newProj
                    self.wbp = wbp.WBP(dataDim[0], dataDim[1], 1)
                else:
                    self.logTiltSeries0 = np.dstack((self.logTiltSeries0,
                                                     newProj))

                self.logFiles = np.append(self.logFiles, file)

            except Exception:
                print('Could not read : {}, will proceed with reconstruction\
                        and re-download on next pass'.format(file))
                break

        # Apply Cross-Correlation after reading images (if selected)
        if self.alignMethod == 'xcor':
            self.logTiltSeries = self.xcorr_align(self.logTiltSeries0)
            # update tilt angles and sinogram
            self.wbp.set_tilt_series(self.logTiltSeries, self.logTiltAngles)
            # re-center tilt axis
            self.logTiltSeries = self.shift_tilt_axis(self.logTiltSeries,
                                                      self.logTiltAngles)
        else:
            self.logTiltSeries = self.logTiltSeries0

    def monitor(self, seconds=1):
        """Return true if, within seconds, any new files exist in listenDir
        NOTE: Lazy Scheme is used (set difference), can update """

        for ts in range(0, seconds):
            self.listenFiles = self.listen_files_list(self.listenDir)
            FoI = list(set(self.listenFiles)-set(self.logFiles))
            if len(FoI) == 0:
                time.sleep(1)
            else:
                self.CHANGE_appendAll() # Can be probamatic for first iter..
                return True

        return False

    def read_projection_image(self, fname):
        """Acquires angles from metadata of .dm4 files"""

        if self.fileExt == 'dm4':
            file = dm.FileDM(fname)
            alphaTag = '.ImageList.2.ImageTags.Microscope Info.'\
                       'Stage Position.Stage Alpha'
            return (file.getDataset(0)['data'], file.allTags[alphaTag])
        elif self.fileExt == 'ser':
            file = ser.SerReader(fname)
            return (file['data'], file['metadata']['Stage A [deg]'])
        # Stage Alpha isn't stored in metadata for dm3 or tif
        elif self.fileExt == 'dm3':
            # Parse fname for stage alpha
            file = dm.FileDM(fname)

            match = 'degrees'
            tags = fname.split('_')

            angle = [tag for tag in tags if match in tag][0]
            angleInd = angle.find(match)
            return (file.getDataset(0)['data'], float(angle[:angleInd]))

    # Pass Tilt Series to Reconstruction Objects
    def load_tilt_series(self, tomo, alg):

        if alg != 'WBP':
            (Nslice, Nray, Nproj) = self.logTiltSeries.shape
            b = np.zeros([Nslice, Nray * Nproj], dtype=np.float32)
            for s in range(Nslice):
                b[s, :] = self.logTiltSeries[s, :, :].transpose().ravel()
            tomo.set_tilt_series(b)
        else:
            tomo.set_tilt_series(self.logTiltSeries, self.logTiltAngles)

    # Remove Background Intensity
    def background_subtract(self, image):

        (Nx, Ny) = image.shape
        XRANGE = np.array([0, int(Nx//16)], dtype=int)
        YRANGE = np.array([0, int(Ny//16)], dtype=int)

        image -= np.average(image[XRANGE[0]:XRANGE[1], YRANGE[0]:YRANGE[1]])

        image[image < 0] = 0

        return image

    # Shift Image so that the center of mass is at the origin"
    # Automatically align tilt images by center of mass method
    def center_of_mass_align(self, image):
        (Nx, Ny) = image.shape
        y = np.linspace(0, Ny-1, Ny)
        x = np.linspace(0, Nx-1, Nx)
        [X, Y] = np.meshgrid(x, y, indexing="ij")

        imageCOM_x = int(np.sum(image * X) / np.sum(image))
        imageCOM_y = int(np.sum(image * Y) / np.sum(image))

        sx = -(imageCOM_x - Nx // 2)
        sy = -(imageCOM_y - Ny // 2)

        output = np.roll(image, sx, axis=0)
        output = np.roll(output, sy, axis=1)

        return output

    # Automatically align tilt images by cross-correlation
    def xcorr_align(self, tiltSeries):

        referenceIndex = tiltSeries.shape[2] // 2

        # create Fourier space filter
        filterCutoff = 4
        (Ny, Nx, Nproj) = tiltSeries.shape
        ky = np.fft.fftfreq(Ny)
        kx = np.fft.fftfreq(Nx)
        [kX, kY] = np.meshgrid(kx, ky)
        kR = np.sqrt(kX**2 + kY**2)
        kFilter = (kR <= (0.5 / filterCutoff)) * \
            np.sin(2 * filterCutoff * np.pi * kR)**2

        # create real sapce filter to remove edge discontinuities
        y = np.linspace(1, Ny, Ny)
        x = np.linspace(1, Nx, Nx)
        [X, Y] = np.meshgrid(x, y)
        rFilter = (np.sin(np.pi * X / Nx) * np.sin(np.pi * Y / Ny)) ** 2

        for i in range(referenceIndex, Nproj - 1):
            tiltSeries[:, :, i + 1] = self.crossCorrelationAlign(
                tiltSeries[:, :, i + 1], tiltSeries[:, :, i], rFilter, kFilter)

        for i in range(referenceIndex, 0, -1):
            tiltSeries[:, :, i - 1] = self.crossCorrelationAlign(
                tiltSeries[:, :, i - 1], tiltSeries[:, :, i], rFilter, kFilter)

        return tiltSeries

    # Align image to reference by cross-correlation
    def crossCorrelationAlign(self, image, reference, rFilter, kFilter):

        image_f = np.fft.fft2((image - np.mean(image)) * rFilter)
        reference_f = np.fft.fft2((reference - np.mean(reference)) * rFilter)

        xcor = abs(np.fft.ifft2(np.conj(image_f) * reference_f * kFilter))
        shifts = np.unravel_index(xcor.argmax(), xcor.shape)

        # shift image
        output = np.roll(np.roll(image, shifts[0], axis=0), shifts[1], axis=1)

        return output

    # Recenter the tilt axis after xcorr
    def shift_tilt_axis(self, tiltSeries, tiltAngles):

        Nx, Ny, Nz = tiltSeries.shape

        shifts = (np.linspace(-20, 20, 41)).astype('int')
        numberOfSlices = 5  # number of slices used for recon

        # randomly choose slices with top 50% total intensities
        tiltSeriesSum = np.sum(tiltSeries, axis=(1, 2))
        temp = tiltSeriesSum.argsort()[Nx // 2:]
        slices = temp[np.random.permutation(temp.size)[:numberOfSlices]]

        I = np.zeros(shifts.size)

        for i in range(shifts.size):

            shiftedTiltSeries = np.roll(
                tiltSeries[slices, :, :, ], shifts[i], axis=1)
            for s in range(numberOfSlices):

                recon = self.wbp.wbp2(shiftedTiltSeries[s, :, :],
                                      'ramp', 'linear')
                I[i] = I[i] + np.amax(recon)

        tiltSeries = np.roll(tiltSeries, shifts[np.argmax(I)], axis=1)

        return tiltSeries
