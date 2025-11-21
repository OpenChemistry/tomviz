import numpy as np
import scipy.sparse as ss
import tomviz.operators
from tomviz.utils import apply_to_each_array
import time


class ReconSirtOperator(tomviz.operators.CompletableOperator):

    @apply_to_each_array
    def transform(self, dataset, Niter=10, stepSize=0.0001,
                  updateMethodIndex=0, Nupdates=0):
        """
        3D Reconstruct from a tilt series using Simultaneous Iterative
        Reconstruction Techniques (SIRT)"""
        self.progress.maximum = 1

        update_methods = ('landweber', 'cimmino', 'component averaging')
        #reference
        """L. Landweber, Amer. J. Math., 73 (1951), pp. 615–624"""
        """G. Cimmino, La Ric. Sci., XVI, Ser. II, Anno IX, 1 (1938),
        pp. 326–333
        """
        """Y. Censor et al, Parallel Comput., 27 (2001), pp. 777–808"""

        # Get Tilt angles
        tiltAngles = dataset.tilt_angles

        #remove zero tilt anlges
        if np.count_nonzero(tiltAngles) < tiltAngles.size:
            tiltAngles = tiltAngles + 0.001

        # Get Tilt Series
        tiltSeries = dataset.active_scalars
        (Nslice, Nray, Nproj) = tiltSeries.shape

        #Check if there's negative values, shift by minimum if true.
        if np.any(tiltSeries < 0):
            tiltSeries -= np.amin(tiltSeries)

        if tiltSeries is None:
            raise RuntimeError("No scalars found!")

        # Determine slice for live updates
        Nupdates = calc_Nupdates(Nupdates, Niter)

        # Generate measurement matrix
        self.progress.message = 'Generating measurement matrix'
        A = parallelRay(Nray, 1.0, tiltAngles, Nray, 1.0) #A is a sparse matrix
        recon = np.zeros([Nslice, Nray, Nray], dtype=np.float32, order='F')

        self.progress.maximum = Nslice*Niter + 1
        step = 0

        #create a reconstruction object
        r = SIRT(A, update_methods[updateMethodIndex], Nslice)
        r.initialize()
        step += 1
        self.progress.value = step

        t0 = time.time()
        counter = 1
        etcMessage = 'Estimated time to complete: n/a'

        #create child for recon
        child = dataset.create_child_dataset()

        for i in range(Niter):

            if self.completed:
                break

            for s in range(Nslice):

                if self.canceled or self.completed:
                    return

                self.progress.message = 'Iteration No.%d/%d,Slice No.%d/%d.' % (
                    i + 1, Niter, s + 1, Nslice) + etcMessage

                b = tiltSeries[s, :, :].transpose().flatten()
                recon_slice = recon[s, :, :].flatten()
                recon[s, :, :] = r.recon2(b, recon_slice, stepSize,
                                          s).reshape((Nray, Nray))

                step += 1
                self.progress.value = step

                timeLeft = (time.time() - t0) / counter * (Nslice*Niter -
                                                           counter)
                counter += 1
                timeLeftMin, timeLeftSec = divmod(timeLeft, 60)
                timeLeftHour, timeLeftMin = divmod(timeLeftMin, 60)
                etcMessage = 'Estimated time to complete: %02d:%02d:%02d' % (
                    timeLeftHour, timeLeftMin, timeLeftSec)

                # Give 4 updates for first iteration.
                if Nupdates != 0 and i == 0 and (s + 1) % (Nslice//4) == 0:
                    child.active_scalars = recon
                    self.progress.data = child

            #Positivity constraint.
            recon[recon < 0] = 0

            #Update at the end of each iteration.
            if Nupdates != 0 and (i + 1) % Nupdates == 0:
                child.active_scalars = recon
                self.progress.data = child

        # One last update of the child data.
        child.active_scalars = recon #add recon to child
        self.progress.data = child

        returnValues = {}
        returnValues["reconstruction"] = child
        return returnValues


class SIRT:

    def __init__(self, A, method, Nslice):
        self.A = A
        self.method = method
        (self.Nrow, self.Ncol) = self.A.shape
        #f = Placeholder for 2d image
        self.f = np.zeros(self.Ncol, dtype=np.float32)

    def initialize(self):
        if self.method == 'landweber':
            self.AT = self.A.transpose()
        elif self.method == 'cimmino':
            self.A = self.A.tocsr()
            self.AT = self.A.transpose()
            rowInnerProduct = np.zeros(self.Nrow, dtype=np.float32)
            self.a = np.zeros(self.Ncol, dtype=np.float32)
            # Calculate row inner product, placeholder for matrix rows
            row = np.zeros(self.Ncol, dtype=np.float32)
            for i in range(self.Nrow):
                row[:] = self.A[i, :].toarray()
                rowInnerProduct[i] = np.dot(row, row)
            self.M = ss.diags(1/rowInnerProduct)
        elif self.method == 'component averaging':
            self.A = self.A.tocsr()
            self.AT = self.A.transpose()
            weightedRowProduct = np.zeros(self.Nrow, dtype=np.float32)
            self.a = np.zeros(self.Ncol, dtype=np.float32)

            # Calculate number of non-zero elements in each column
            s = np.zeros(self.Ncol, dtype=np.float32)

            for i in range(self.Ncol):
                s[i] = self.A[:, i].count_nonzero()

            # Calculate weighted row product
            row = np.zeros(self.Ncol) #placeholder for matrix rows
            for i in range(self.Nrow):
                row[:] = self.A[i, :].toarray()
                weightedRowProduct[i] = np.sum(row * row * s)
            self.M = ss.diags(1/weightedRowProduct)
        else:
            print("Invalid update method!")

    def recon2(self, b, x, stepSize, index):
        self.f[:] = x
        if self.method == 'landweber':
            a = self.AT.dot(b - self.A.dot(self.f))
            self.f = self.f + a * stepSize
        elif self.method == 'cimmino':
            self.a[:] = 0
            g = self.M.dot(b - self.A.dot(self.f))
            self.a = self.AT.dot(g)
            self.f = self.f + self.a * stepSize / self.Nrow
        elif self.method == 'component averaging':
            self.a[:] = 0
            g = self.M.dot(b - self.A.dot(self.f))
            self.a = self.AT.dot(g)
            self.f = self.f + self.a * stepSize
        else:
            print("Invalid update method!")
        return self.f


def parallelRay(Nside, pixelWidth, angles, Nray, rayWidth):
    # Suppress warning messages that pops up when dividing zeros
    np.seterr(all='ignore')
    Nproj = angles.size # Number of projections

    # Ray coordinates at 0 degrees.
    offsets = np.linspace(-(Nray * 1.0 - 1) / 2,
                          (Nray * 1.0 - 1) / 2, Nray) * rayWidth
    # Intersection lines/grid Coordinates
    xgrid = np.linspace(-Nside * 0.5, Nside * 0.5, Nside + 1) * pixelWidth
    ygrid = np.linspace(-Nside * 0.5, Nside * 0.5, Nside + 1) * pixelWidth
    # Initialize vectors that contain matrix elements and corresponding
    # row/column numbers
    rows = np.zeros((2 * Nside * Nproj * Nray), dtype=np.float32)
    cols = np.zeros((2 * Nside * Nproj * Nray), dtype=np.float32)
    vals = np.zeros((2 * Nside * Nproj * Nray), dtype=np.float32)
    idxend = 0

    for i in range(0, Nproj): # Loop over projection angles
        ang = angles[i] * np.pi / 180.
        # Points passed by rays at current angles
        xrayRotated = np.cos(ang) * offsets
        yrayRotated = np.sin(ang) * offsets
        xrayRotated[np.abs(xrayRotated) < 1e-8] = 0
        yrayRotated[np.abs(yrayRotated) < 1e-8] = 0

        a = -np.sin(ang)
        a = rmepsilon(a)
        b = np.cos(ang)
        b = rmepsilon(b)

        for j in range(0, Nray): # Loop rays in current projection
            #Ray: y = tx * x + intercept
            t_xgrid = (xgrid - xrayRotated[j]) / a
            y_xgrid = b * t_xgrid + yrayRotated[j]

            t_ygrid = (ygrid - yrayRotated[j]) / b
            x_ygrid = a * t_ygrid + xrayRotated[j]
            # Collect all points
            t_grid = np.append(t_xgrid, t_ygrid)
            xx = np.append(xgrid, x_ygrid)
            yy = np.append(y_xgrid, ygrid)
            # Sort the coordinates according to intersection time
            I = np.argsort(t_grid)
            xx = xx[I]
            yy = yy[I]

            # Get rid of points that are outside the image grid
            Ix = np.logical_and(xx >= -Nside / 2.0 * pixelWidth,
                                xx <= Nside / 2.0 * pixelWidth)
            Iy = np.logical_and(yy >= -Nside / 2.0 * pixelWidth,
                                yy <= Nside / 2.0 * pixelWidth)
            I = np.logical_and(Ix, Iy)
            xx = xx[I]
            yy = yy[I]

            # If the ray pass through the image grid
            if (xx.size != 0 and yy.size != 0):
                # Get rid of double counted points
                I = np.logical_and(np.abs(np.diff(xx)) <=
                                   1e-8, np.abs(np.diff(yy)) <= 1e-8)
                I2 = np.zeros(I.size + 1)
                I2[0:-1] = I
                xx = xx[np.logical_not(I2)]
                yy = yy[np.logical_not(I2)]

                # Calculate the length within the cell
                length = np.sqrt(np.diff(xx)**2 + np.diff(yy)**2)
                #Count number of cells the ray passes through
                numvals = length.size

                # Remove the rays that are on the boundary of the box in the
                # top or to the right of the image grid
                check1 = np.logical_and(b == 0, np.absolute(
                    yrayRotated[j] - Nside / 2 * pixelWidth) < 1e-15)
                check2 = np.logical_and(a == 0, np.absolute(
                    xrayRotated[j] - Nside / 2 * pixelWidth) < 1e-15)
                check = np.logical_not(np.logical_or(check1, check2))

                if np.logical_and(numvals > 0, check):
                    # Calculate corresponding indices in measurement matrix
                    # First, calculate the mid points coord. between two
                    # adjacent grid points
                    midpoints_x = rmepsilon(0.5 * (xx[0:-1] + xx[1:]))
                    midpoints_y = rmepsilon(0.5 * (yy[0:-1] + yy[1:]))
                    #Calculate the pixel index for mid points
                    pixelIndicex = \
                        (np.floor(Nside / 2.0 - midpoints_y / pixelWidth)) * \
                        Nside + (np.floor(midpoints_x /
                                          pixelWidth + Nside / 2.0))
                    # Create the indices to store the values to the measurement
                    # matrix
                    idxstart = idxend
                    idxend = idxstart + numvals
                    idx = np.arange(idxstart, idxend)
                    # Store row numbers, column numbers and values
                    rows[idx] = i * Nray + j
                    cols[idx] = pixelIndicex
                    vals[idx] = length
            else:
                print("Ray No. %d at %f degree is out of image grid!" %
                      (j + 1, angles[i]))

    # Truncate excess zeros.
    rows = rows[:idxend]
    cols = cols[:idxend]
    vals = vals[:idxend]
    A = ss.coo_matrix((vals, (rows, cols)), shape=(Nray * Nproj, Nside**2),
                      dtype=np.float32)
    return A


def rmepsilon(input):
    if (input.size > 1):
        input[np.abs(input) < 1e-10] = 0
    else:
        if np.abs(input) < 1e-10:
            input = 0
    return input


def calc_Nupdates(Nupdates, Niter):
    if Nupdates == 0:
        Nupdates = 0
    #If the user selects 100%, update for every slice.
    elif Nupdates == 100:
        Nupdates = 1
    else:
        Nupdates = int(round((Niter*(1 - Nupdates/100))))
    return Nupdates
