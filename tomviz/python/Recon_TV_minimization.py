import numpy as np
import scipy.sparse as ss
from tomviz import utils
import tomviz.operators
import time


class ReconTVOperator(tomviz.operators.CancelableOperator):

    def transform_scalars(self, dataset, Niter=1):
        """3D Reconstruct from a tilt series using simple TV minimzation"""
        self.progress.maximum = 1

        # Get Tilt angles
        tiltAngles = utils.get_tilt_angles(dataset)

        #remove zero tilt anlges
        if np.count_nonzero(tiltAngles) < tiltAngles.size:
            tiltAngles = tiltAngles + 0.001

        # Get Tilt Series
        tiltSeries = utils.get_array(dataset)
        (Nslice, Nray, Nproj) = tiltSeries.shape

        if tiltSeries is None:
            raise RuntimeError("No scalars found!")

        # Generate measurement matrix
        A = parallelRay(Nray, 1.0, tiltAngles, Nray, 1.0) #A is a sparse matrix
        recon = np.empty([Nslice, Nray, Nray], dtype=float, order='F')
        A = A.todense()

        (Nslice, Nray, Nproj) = tiltSeries.shape
        (Nrow, Ncol) = A.shape
        rowInnerProduct = np.zeros(Nrow)
        row = np.zeros(Ncol)
        f = np.zeros(Ncol) # Placeholder for 2d image

        alpha = 0.2
        ng = 30
        beta_red = 0.995
        beta = 1.0
        # Calculate row inner product, preparation for ART recon
        for j in range(Nrow):
            row[:] = A[j, ].copy()
            rowInnerProduct[j] = np.dot(row, row)

        self.progress.maximum = Niter
        step = 0
        t0 = time.time()
        etcMessage = 'Estimated time to complete: n/a'
        for i in range(Niter): #main loop
            if self.canceled:
                return
            self.progress.message = 'Iteration No.%d/%d. ' % (
                i + 1, Niter) + etcMessage
            recon_temp = recon.copy()
            #ART recon
            for s in range(Nslice): #
                f[:] = 0
                b = tiltSeries[s, :, :].transpose().flatten()
                for j in range(Nrow):
                    row[:] = A[j, ].copy()
                    row_f_product = np.dot(row, f)
                    a = (b[j] - row_f_product) / rowInnerProduct[j]
                    f = f + row * a * beta
                recon[s, :, :] = f.reshape((Nray, Nray))

            recon[recon < 0] = 0 #Positivity constraint

            #calculate tomogram change due to POCS
            dPOCS = np.linalg.norm(recon_temp - recon)

            #3D TV minimization
            for j in range(ng):
                r = np.lib.pad(recon, ((1, 1), (1, 1), (1, 1)), 'edge')
                v1n = 3 * r - np.roll(r, 1, axis=0) - \
                                      np.roll(r, 1, axis=1) - np.roll(r, 1, axis=2) # noqa TODO reformat this
                v1d = np.sqrt(1e-8 + (r - np.roll(r, 1, axis=0))**2 + (r -
                              np.roll(r, 1, axis=1))**2 + (r - np.roll(r, 1, axis=2))**2) # noqa TODO reformat this

                v2n = r - np.roll(r, -1, axis=0)
                v2d = np.sqrt(1e-8 + (np.roll(r, -1, axis=0) - r)**2 +
                        (np.roll(r, -1, axis=0) -  # noqa TODO reformat this
                         np.roll(np.roll(r, -1, axis=0), 1, axis=1))**2 +
                        (np.roll(r, -1, axis=0) - np.roll(np.roll(r, -1, axis=0), 1, axis=2))**2) # noqa TODO reformat this

                v3n = r - np.roll(r, -1, axis=1)
                v3d = np.sqrt(1e-8 + (np.roll(r, -1, axis=1) - np.roll(np.roll(r, -1, axis=1), 1, axis=0))**2 + # noqa TODO reformat this
                              (np.roll(r, -1, axis=1) - r)**2 + # noqa TODO reformat this
                              (np.roll(r, -1, axis=1) - np.roll(np.roll(r, -1, axis=1), 1, axis=2))**2) # noqa TODO reformat this

                v4n = r - np.roll(r, -1, axis=2)
                v4d = np.sqrt(1e-8 + (np.roll(r, -1, axis=2) - np.roll(np.roll(r, -1, axis=2), 1, axis=0))**2 + # noqa TODO reformat this
                              (np.roll(r, -1, axis=2) -  # noqa TODO reformat this
                              np.roll(np.roll(r, -1, axis=1), 1, axis=1))**2 +
                              (np.roll(r, -1, axis=2) - r)**2) # noqa TODO reformat this

                v = v1n / v1d + v2n / v2d + v3n / v3d + v4n / v4d
                v = v[1:-1, 1:-1, 1:-1]
                v = v / np.linalg.norm(v)
                recon[:] = recon - alpha * dPOCS * v

            #adjust parameters
            beta = beta * beta_red
            step += 1
            self.progress.value = step

            timeLeft = (time.time() - t0) / counter * \
                (Nslice * Niter - counter)
            counter += 1
            timeLeftMin, timeLeftSec = divmod(timeLeft, 60)
            timeLeftHour, timeLeftMin = divmod(timeLeftMin, 60)
            etcMessage = 'Estimated time to complete: %02d:%02d:%02d' % (
                timeLeftHour, timeLeftMin, timeLeftSec)

        # Set the result as the new scalars.
        utils.set_array(dataset, recon)

        # Mark dataset as volume
        utils.mark_as_volume(dataset)


def tv_minimization(A, tiltSeries, recon, iterNum=1):
    (Nslice, Nray, Nproj) = tiltSeries.shape

    (Nrow, Ncol) = A.shape
    rowInnerProduct = np.zeros(Nrow)
    row = np.zeros(Ncol)
    f = np.zeros(Ncol) # Placeholder for 2d image

    alpha = 0.2
    ng = 30
    beta_red = 0.995
    beta = 1.0
    # Calculate row inner product, preparation for ART recon
    for j in range(Nrow):
        row[:] = A[j, ].copy()
        rowInnerProduct[j] = np.dot(row, row)

    for i in range(iterNum): #main loop
        recon_temp = recon.copy()
        #ART recon
        for s in range(Nslice): #
            f[:] = 0
            b = tiltSeries[s, :, :].transpose().flatten() #get current sinogram
            for j in range(Nrow):
                row[:] = A[j, ].copy()
                row_f_product = np.dot(row, f)
                a = (b[j] - row_f_product) / rowInnerProduct[j]
                f = f + row * a * beta
            recon[s, :, :] = f.reshape((Nray, Nray))

        recon[recon < 0] = 0 #Positivity constraint

        #calculate tomogram change due to POCS
        dPOCS = np.linalg.norm(recon_temp - recon)

        #3D TV minimization
        for j in range(0, ng):
            r = np.lib.pad(recon, ((1, 1), (1, 1), (1, 1)), 'edge')
            v1n = 3 * r - np.roll(r, 1, axis=0) - \
                                  np.roll(r, 1, axis=1) - np.roll(r, 1, axis=2) # noqa TODO reformat this
            v1d = np.sqrt(1e-8 + (r - np.roll(r, 1, axis=0))**2 + (r -
                          np.roll(r, 1, axis=1))**2 + (r - np.roll(r, 1, axis=2))**2) # noqa TODO reformat this

            v2n = r - np.roll(r, -1, axis=0)
            v2d = np.sqrt(1e-8 + (np.roll(r, -1, axis=0) - r)**2 +
                          (np.roll(r, -1, axis=0) -  # noqa TODO reformat this
                             np.roll(np.roll(r, -1, axis=0), 1, axis=1))**2 +
                          (np.roll(r, -1, axis=0) - np.roll(np.roll(r, -1, axis=0), 1, axis=2))**2) # noqa TODO reformat this

            v3n = r - np.roll(r, -1, axis=1)
            v3d = np.sqrt(1e-8 + (np.roll(r, -1, axis=1) - np.roll(np.roll(r, -1, axis=1), 1, axis=0))**2 + # noqa TODO reformat this
                          (np.roll(r, -1, axis=1) - r)**2 + # noqa TODO reformat this
                          (np.roll(r, -1, axis=1) - np.roll(np.roll(r, -1, axis=1), 1, axis=2))**2) # noqa TODO reformat this

            v4n = r - np.roll(r, -1, axis=2)
            v4d = np.sqrt(1e-8 + (np.roll(r, -1, axis=2) - np.roll(np.roll(r, -1, axis=2), 1, axis=0))**2 + # noqa TODO reformat this
                          (np.roll(r, -1, axis=2) -  # noqa TODO reformat this
                             np.roll(np.roll(r, -1, axis=1), 1, axis=1))**2 +
                          (np.roll(r, -1, axis=2) - r)**2) # noqa TODO reformat this

            v = v1n / v1d + v2n / v2d + v3n / v3d + v4n / v4d
            v = v[1:-1, 1:-1, 1:-1]
            v = v / np.linalg.norm(v)
            recon = recon - alpha * dPOCS * v

        #adjust parameters
        beta = beta * beta_red


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
    #print xgrid
    #print ygrid
    # Initialize vectors that contain matrix elements and corresponding
    # row/column numbers
    rows = np.zeros(2 * Nside * Nproj * Nray)
    cols = np.zeros(2 * Nside * Nproj * Nray)
    vals = np.zeros(2 * Nside * Nproj * Nray)
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
            #print xrayRotated[j],yrayRotated[j]
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
                    #print 'midpoints_x is:',midpoints_x
                    #print 'midpoints_y is:',midpoints_y
                    #Calculate the pixel index for mid points
                    pixelIndicex = ((np.floor(Nside / 2.0 - midpoints_y / pixelWidth)) * # noqa TODO reformat this
                        Nside + (np.floor(midpoints_x /
                        pixelWidth + Nside / 2.0)))
                    #print 'pixelIndicex is:', pixelIndicex
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
                print(("Ray No.", j + 1, "at", angles[i],
                       "degree is out of image grid!"))
    # Truncate excess zeros.
    rows = rows[:idxend]
    cols = cols[:idxend]
    vals = vals[:idxend]
    A = ss.coo_matrix((vals, (rows, cols)), shape=(Nray * Nproj, Nside**2))
    return A


def rmepsilon(input):
    if (input.size > 1):
        input[np.abs(input) < 1e-10] = 0
    else:
        if np.abs(input) < 1e-10:
            input = 0
    return input
