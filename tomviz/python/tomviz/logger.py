from tomviz.io import dm, ser
import numpy as np
import time, os

# Class for listening and logging new files for real-time reconstruction
class logger:

    def __init__(self, listenDirectory, fileExtension):

        # Create Local Directory 
        if not os.path.exists(listenDirectory):
            os.makedirs(listenDirectory)

        # Set Member Variables
        self.listenDir = listenDirectory
        self.fileExt = fileExtension

        self.listen_files = self.listen_files_list(self.listenDir)
        self.log_files, self.log_projs, self.log_tilts = [], [], []
        print("Listener on {} created.".format(self.listenDir))

    def listen_files_list(self, directory):
        """Grab current files in listen directory"""
        files = [f for f in os.listdir(directory) if f[-len(self.fileExt):] == self.fileExt]
        files.sort(key = lambda x: x[:3])
        return files

    def CHANGE_appendAll(self):
        """Append stored files for each new element"""
        # Separate new files to be loaded
        FoI = list(set(self.listen_files)-set(self.log_files))
        FoI.sort(key = lambda x: x[:3])
        for file in FoI:
            print("Loading {}".format(file))
            filePath = "{}/{}".format(self.listenDir, file)

            try: 

                (newProj, newAngle) = self.read_projection_image(filePath)

                self.log_tilts = np.append(self.log_tilts, newAngle)

                newProj = self.center_of_mass_align(self.background_subtract(newProj))

                # Account for Python's disdain for AxAx1 arrays 
                # (compresses to 2D)
                if(len(self.log_projs) == 0):
                    dataDim = np.shape(newProj)
                    self.log_projs = np.zeros([dataDim[0],dataDim[1],1])
                    self.log_projs[:,:,0] = newProj
                else:
                    self.log_projs = np.dstack((self.log_projs, newProj))
                    np.save('tiltSeries.npy', self.log_projs)

                self.log_files = np.append(self.log_files, file)

            except: 
                print('Could not read : {}, will preceed with reconstruction\
                        and re-download on next pass'.format(file))
                break

    def monitor(self, seconds=1):
        """Return true if, within seconds, any new files exist in listenDir
        NOTE: Lazy Scheme is used (set difference), can update """

        for ts in range(0,seconds):
            self.listen_files = self.listen_files_list(self.listenDir)
            FoI = list(set(self.listen_files)-set(self.log_files))
            if len(FoI) == 0:
                time.sleep(1)
            else:
                self.CHANGE_appendAll() # Can be probamatic for first iter..
                return True;

        return False

    def read_projection_image(self, fname):
        """Acquires angles from metadata of .dm4 files"""

        if self.fileExt == 'dm4':
            file = dm.fileDM(fname)
            alphaTag = ".ImageList.2.ImageTags.Microscope Info.Stage Position.Stage Alpha"
            return (file.getDataset(0)['data'], file.allTags[alphaTag])
        elif self.fileExt == 'ser':
            file = ser.serReader(fname)
            return (file['data'], file['metadata']['Stage A [deg]'])
        else: # Stage Alpha isn't stored in metadata for dm3 or tif
              # Parse fname for stage alpha
              print('<<<<<TBD>>>>>')

    def load_tilt_series(self, tomo, alg):

        if alg != 'WBP':
            (Nslice, Nray, Nproj) = self.log_projs.shape
            b = np.zeros([Nslice, Nray * Nproj], dtype=np.float32)
            for s in range(Nslice):
                b[s, :] = self.log_projs[s, :, :].transpose().ravel()
            tomo.set_tilt_series(b)
        else:
            tomo.set_tilt_series(self.log_projs, self.log_tilts)

    # Shift Image so that the center of mass is at the origin"
    # Automatically align tilt images by center of mass method
    def center_of_mass_align(self, image):
        (Nx, Ny) = image.shape
        y = np.linspace(0,Ny-1,Ny)
        x = np.linspace(0,Nx-1,Nx)
        [X, Y] = np.meshgrid(x, y, indexing="ij")

        imageCOM_x = int(np.sum(image * X) / np.sum(image))
        imageCOM_y = int(np.sum(image * Y) / np.sum(image))

        sx = -(imageCOM_x - Nx // 2)
        sy = -(imageCOM_y - Ny // 2)

        output = np.roll(image, sx, axis=0)
        output = np.roll(output, sy, axis=1)

        return output

    # Remove Background Intensity
    def background_subtract(self, image):

        (Nx, Ny) = image.shape
        XRANGE = np.array([0, int(Nx//16)], dtype=int)
        YRANGE = np.array([0, int(Ny//16)], dtype=int)

        image -= np.average(image[XRANGE[0]:XRANGE[1], YRANGE[0]:YRANGE[1]])

        image[image < 0] = 0

        return image
