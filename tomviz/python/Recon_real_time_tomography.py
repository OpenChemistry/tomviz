import tomviz._wrapping as wrap
from tomviz import logger, wbp
from tomviz import pytvlib
import tomviz.operators
import numpy as np
import time


class RealTimeTomography(tomviz.operators.CancelableOperator):

    def transform(self, dataset, localDirectory=None, alg=None,
                  maxIter=None, fileExt=None):

        # Parse Inputs
        fileExtensions = ('dm4', 'dm3', 'ser', 'tif')
        algorithms = ('ART', 'randART', 'SIRT', 'WBP')
        alg = algorithms[alg]

        # Boolean to Determine Whether Algorithm is ART.
        artBool = alg == 'ART' or alg == 'randART'

        # Create child for recon
        child = dataset.create_child_dataset()

        # Logger to Read Directory
        tomoLogger = logger.logger(localDirectory, fileExtensions[fileExt])

        # Keep Checking Directory until a projection is read.
        while True:
            if tomoLogger.monitor():
                break

        (Nslice, Nray, Nproj) = tomoLogger.log_projs.shape

        # Initialize C++ Object..
        if alg != 'WBP':
            tomo = wrap.ctvlib(Nslice, Nray, Nproj)
        else:
            tomo = wbp.wbp(Nslice, Nray, Nproj)
            beta = 0
            maxIter = Nslice
        tomoLogger.load_tilt_series(tomo, alg)

        # Progress Bar
        self.progress.maximum = maxIter

        # Generate measurement matrix (if Iterative Algorithm)
        self.progress.message = 'Generating measurement matrix'
        pytvlib.initialize_algorithm(tomo, alg, Nray, tomoLogger.log_tilts)

        # Descent Parameter Initialization
        if alg == 'SIRT':
            beta = 1/tomo.lipschits()
        elif artBool:
            beta0 = 0.5
            beta = beta0
            betaRed = 0.99

        # Dynamic Tilt Series Loop.
        while True:

            t0 = time.time()
            counter = 1
            etcMessage = 'Estimated time to complete: n/a'

            #Main Reconstruction Loop
            for jj in range(maxIter):

                if self.canceled:
                    break

                self.progress.message = 'Iteration No.%d/%d. '\
                    % (jj + 1, maxIter) + etcMessage

                # Run Reconstruction Algorithm
                pytvlib.run(tomo, alg, beta, jj)

                # Decay Descent Parameter if ART.
                if artBool:
                    beta *= betaRed

                # Progress Bar
                self.progress.value = jj + 1
                (etcMessage, counter) = pytvlib.timer(t0, counter, maxIter)

            # Return a Current Iterate
            self.progress.message = 'Updating Tomogram Visualization'
            if alg != 'WBP':
                child.active_scalars = pytvlib.get_recon((Nslice, Nray), tomo)
            else:
                child.active_scalars = tomo.recon
            self.progress.data = child

            # Run Logger to see how many projections were collected since
            # last check.
            if tomoLogger.monitor():

                # Update tomo (C++) with new projections / tilt Angles.
                self.progress.message = 'Generating measurement matrix'
                prevTilt = np.int(tomoLogger.log_tilts[-1])
                pytvlib.initialize_algorithm(tomo, alg, Nray,
                                             tomoLogger.log_tilts, prevTilt)
                tomoLogger.load_tilt_series(tomo, alg)

                # Recalculate Lipschitz Constant or Reset Descent Parameter
                if alg == 'SIRT':
                    beta = 1/tomo.lipschits()
                elif artBool:
                    beta = beta0

        # One last update of the child data.
        child.active_scalars = pytvlib.get_recon((Nslice, Nray), tomo)
        self.progress.data = child

        returnValues = {}
        returnValues["reconstruction"] = child
        return returnValues
