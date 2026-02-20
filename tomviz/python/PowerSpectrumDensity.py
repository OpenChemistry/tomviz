import tomviz.operators
import tomviz.utils

import numpy as np
import scipy.stats as stats

def pad_to_cubic(arr):
    """
    Pads a 3D numpy array to make it cubic (all dimensions equal to the largest dimension).
    The padding is added to the end of each axis.

    Parameters:
        arr (np.ndarray): Input 3D array.

    Returns:
        np.ndarray: Cubic padded array.
    """
    max_dim = max(arr.shape)
    pad_widths = [(0, max_dim - s) for s in arr.shape]
    return np.pad(arr, pad_widths, mode='constant', constant_values=0)

def psd3D(image, pixel_size):
    #again likes square images, you might need to pad the image to make it work.
    fourier_image = np.fft.fftn(image)
    fourier_amplitudes = np.abs(fourier_image)**2
    npix = image.shape[0]
    kfreq = np.fft.fftfreq(npix) * npix
    kfreq3D = np.meshgrid(kfreq, kfreq, kfreq)
    knrm = np.sqrt(kfreq3D[0]**2 + kfreq3D[1]**2 + kfreq3D[2]**2)

    knrm = knrm.flatten()
    fourier_amplitudes = fourier_amplitudes.flatten()

    kbins = np.arange(0.5, npix//2+1, 1.)
    kvals = 0.5 * (kbins[1:] + kbins[:-1])
    Abins, _, _ = stats.binned_statistic(knrm, fourier_amplitudes,
                                     statistic = "mean",
                                     bins = kbins)
    Abins *= np.pi * (kbins[1:]**2 - kbins[:-1]**2)
    x = kvals/(npix*pixel_size)
    return x, Abins


class PoreSizeDistribution(tomviz.operators.CancelableOperator):
    def transform(self, dataset):
        scalars = dataset.active_scalars

        if scalars is None:
            raise RuntimeError("No scalars found!")

        scalars = pad_to_cubic(scalars)

        return_values = {}

        column_names = ["x", "PSD"]
        x, bins = psd3D(scalars, dataset.spacing[0])

        n = len(x)

        table_data = np.empty(shape=(n, 2))

        table_data[:, 0] = x
        table_data[:, 1] = bins

        table = tomviz.utils.make_spreadsheet(column_names, table_data, ("Spatial Frequency", "Power Spectrum Density"), (False, True))
        return_values["plot"] = table

        return return_values
