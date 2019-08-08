#cython: initializedcheck=False
#cython: wraparound=False
# cython: boundscheck=False
#cython: cdivision=True
import numpy as np
cimport numpy as np
cimport cython
@cython.boundscheck(False)

# ctypedef np.float64_t Image

def calculate_weight_fast(pixel_patches, neighbors_patches, n_small, h, a):
    # set the similarity defined by Euclidean distance
    cdef np.float64_t[:] similarities = np.sum(np.subtract(pixel_patches, neighbors_patches)**2, axis=1)
    similarities = np.multiply(1.0 / ((2*n_small+1)**2), similarities)

    # element wise set to zero if below 0
    cdef np.int64_t i
    for i in range(len(similarities)):
        if (similarities[i] - 2*a**2) < 0:
            similarities[i] = 0

    # calculate weight
    cdef np.float64_t[:] weights = np.exp(np.negative(similarities) / h**2)

    # compute contributions
    cdef np.float64_t[:] neighbor_pixels = neighbors_patches[:, int(neighbors_patches.shape[1]/2)]
    cdef np.float64_t[:] contributions = np.multiply(weights, neighbor_pixels)

    # return the weight and the contribution it would make to denoising
    return weights, contributions

# Non Local Mean filter
# n_large: search square window size of neighboring pixel j
# n_small: square patch size
# h: degree of filtering
# a: Gaussian standard deviation
# @profile
def _nonlocalmeans_2D_fast(in_image, n_large, n_small, h, a):

    if (len(in_image.shape) != 2):
        print("Error: input slice image dimension is not 2D")
        return in_image

    # initialize output image
    cdef np.float64_t[:, :] out_image = np.zeros_like(in_image)
    # cdef np.ndarray[np.float64_t, ndim=2] out_image = np.zeros([in_image.shape[0], in_image.shape[1]], dtype=np.float64)

    # some info about input image
    cdef int num_rows = in_image.shape[0]
    cdef int num_columns = in_image.shape[1]
    print("in_image has " + str(num_rows) + " rows, " + str(num_columns) + " columns")

    # precompute coordinate difference for the neighbors
    # neighbor_range = range(-n_large, n_large + 1)
    # exclude the (0, 0) point which would be the original point
    cdef np.int64_t[:, :] neighbor_window = np.array([(row, column) for row in range(-n_large, n_large + 1) for column in range(-n_large, n_large + 1) if not (row == 0 and column == 0)])
    cdef np.int64_t[:] neighbor

    # precompute coordinate difference for the individual patch that would be used to calculate weight
    cdef np.int64_t[:] patch_rows = (np.indices((2*n_small+1, 2*n_small+1))-n_small)[0].flatten()
    cdef np.int64_t[:] patch_cols = (np.indices((2*n_small+1, 2*n_small+1))-n_small)[1].flatten()

    # padding for denoising "corner" pixels
    cdef np.int64_t padding_length = n_large + n_small
    cdef np.float64_t[:, :] padded_image = np.pad(in_image, padding_length, mode='wrap')

    # variables declarations
    # all the row and column index of the patch around current pixel
    cdef np.int64_t[:] rows, cols
    # pixel values of the patch around current pixel
    cdef np.float64_t[:] pixel_patch_values_single, neighbors_patch_values_single
    # stack pixel_patch_values_single
    # values of all the neighboring pixel patches
    cdef np.float64_t[:, :] pixel_patch_values, neighbors_patch_values

    # all the individual weights and weights*pixels
    cdef np.float64_t[:] weights, contributions
    # sum of weights and contributions
    cdef np.float64_t Z, C

    # variables used in loops
    cdef np.int64_t i, j, n, m
    cdef int first_time = 1

    # iterate through all pixels, j is rows, i is columns
    # offset is the padding_length so we are still iterating within in_image
    for j in range(padding_length, num_rows + padding_length):
        for i in range(padding_length, num_columns + padding_length):
            # progress monitor
            if ((j-padding_length)%1 == 0 and (i-padding_length) == 0):
                print("row = " + str(j-padding_length) + ", column = " + str(i-padding_length))

            # patch around the current pixel, flattened to 1D
            rows = np.array(np.add(j, patch_rows))
            cols = np.array(np.add(i, patch_cols))
            pixel_patch_values_single = np.array([padded_image[rows[k], cols[k]] for k in range(len(rows))])

            # repeat the same row num_neighbor times
            pixel_patch_values = np.array([pixel_patch_values_single,]*len(neighbor_window))

            # generate neighbor patches
            if (first_time):
                neighbors_patch_values = np.zeros_like(pixel_patch_values)
                first_time = 0

            # patches around every neighbor location of the current pixel
            for n in range((2*n_large+1)**2):
                for neighbor in neighbor_window:
                    # print(np.array([np.add(j+neighbor[0], patch_rows)]))
                    rows = np.array(np.add(j+neighbor[0], patch_rows))
                    cols = np.array(np.add(i+neighbor[1], patch_cols))
                    neighbors_patch_values_single = np.array([padded_image[rows[k], cols[k]] for k in range(len(rows))])
                    for m in range ((2*n_small+1)**2):
                        neighbors_patch_values[n, m] = neighbors_patch_values_single[m]

            # calculate the weights
            weights, contributions = calculate_weight_fast(pixel_patch_values, neighbors_patch_values, n_small, h, a)

            # Z is the sum of all weights, C is the sum of weight*neighbor_pixel, as indicated in paper
            Z = np.sum(weights)
            C = np.sum(contributions)
            out_image[j-padding_length, i-padding_length] = C / Z

    return out_image

