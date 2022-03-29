import tomviz._realtime.ctvlib as ctvlib
from tomviz._realtime import logger, wbp
from tomviz._realtime import pytvlib
import tomviz.operators
import time


class RealTimeTomography(tomviz.operators.CompletableOperator):

    def transform(self, dataset, localDirectory=None, alg=None, invert=None,
                  align=None, maxIter=None, fileExt=None):

        # Parse Inputs
        fileExtensions = ('dm4', 'dm3', 'ser')
        alignments = ('CoM', 'xcor')
        algorithms = ('ART', 'randART', 'SIRT', 'WBP')
        alg = algorithms[alg]

        # Boolean to Determine Whether Algorithm is ART.
        artBool = alg == 'ART' or alg == 'randART'

        # Create child for recon
        child = dataset.create_child_dataset()

        # Logger to Read Directory
        tomoLogger = logger.Logger(localDirectory, fileExtensions[fileExt],
                                   alignments[align], invert)

        # Keep Checking Directory until a projection is read.
        while True:
            if tomoLogger.monitor():
                break

        (Nslice, Nray, Nproj) = tomoLogger.logTiltSeries.shape

        # Initialize C++ Object..
        if alg != 'WBP':
            tomo = ctvlib.ctvlib(Nslice, Nray, Nproj)
        else:
            tomo = wbp.WBP(Nslice, Nray, Nproj)
            beta = 0
            maxIter = Nslice
        tomoLogger.load_tilt_series(tomo, alg)

        # Progress Bar
        self.progress.maximum = maxIter

        # Generate measurement matrix (if Iterative Algorithm)
        self.progress.message = 'Generating measurement matrix'
        pytvlib.initialize_algorithm(tomo, alg, Nray, tomoLogger.logTiltAngles)

        # Descent Parameter Initialization
        if alg == 'SIRT':
            beta = 1/tomo.lipschits()
        elif artBool:
            beta0 = 0.5
            beta = beta0
            betaRed = 0.99

        # Dynamic Tilt Series Loop.
        runExperiment = True
        while runExperiment:

            t0 = time.time()
            counter = 1
            etcMessage = 'Estimated time to complete: n/a'

            #Main Reconstruction Loop
            for jj in range(maxIter):

                # Check to see experiment is complete
                if self.completed or self.canceled:
                    runExperiment = False
                    break

                self.progress.message = 'Reconstructing Tilt Angles (%d'\
                                        ' Sampled): Iteration No.%d/%d. '\
                    % (tomoLogger.logTiltAngles.shape[0], jj + 1,
                        maxIter) + etcMessage

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
                pytvlib.initialize_algorithm(tomo, alg, Nray,
                                             tomoLogger.logTiltAngles, 1)
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
