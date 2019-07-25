# from tomviz import utils
import numpy as np
import math

# helper function that computes MSE between two images
def compute_MSE(image1, image2):
    if (image1.shape != image2.shape):
        print('Error: cannot compute MSE between two images with different sizes')
        return -1

    mse = np.sum((image1-image2)**2)
    mse /= float(image1.shape[0]*image1.shape[1])

    return mse

def compute_PSNR(original, processed, peak=100):
    mse = compute_MSE(original, processed)
    psnr = 10*np.log10(peak*peak/mse)

    return psnr

# the helper function calculates the weight for patch that is centered at (i, j) in in_image
def calculate_weight(in_image, i, j, x, y, n_small, h, a):
    num_rows = in_image.shape[0]
    num_columns = in_image.shape[1]

    similarity = 0

    for row in range(-n_small, n_small+1):
        for column in range(-n_small, n_small+1):
            if (row+j >= 0 and row+j < num_rows and column+i >= 0 and column+i < num_columns
            and
            row+y >= 0 and row+y < num_rows and column+x >= 0 and column+x < num_columns):

                similarity += (in_image[row+j][column+i] - in_image[row+y][column+x])**2

    similarity *= (1.0 / ((2*n_small+1)**2))
    weight = math.exp(-max((similarity - 2*a**2), 0.0) / h**2)

    return weight
# @profile
def calculate_weight_fast(values, pixel_patch, n_small, h, a):
    # get the current input
    pixel_patch = np.array(pixel_patch)
    neighbor_patch = np.array(values[0])

    # set the similarity defined by Euclidean distance
    similarity = np.sum((pixel_patch - neighbor_patch)**2)
    similarity *= (1.0 / ((2*n_small+1)**2))

    # calculate weight
    weight = math.exp(-max((similarity-2*a**2), 0.0) / h**2)

    # return the weight and the contribution it would make to denoising
    return weight, weight*neighbor_patch[int(len(neighbor_patch)/2)]

# @profile
def calculate_weight_fast2(pixel_patch, neighbor_patch, n_small, h, a):
    # set the similarity defined by Euclidean distance
    similarity = np.sum((pixel_patch - neighbor_patch)**2, axis=1)
    similarity *= (1.0 / ((2*n_small+1)**2))
    # element wise set to zero if below 0
    threshold = (similarity - 2*a**2) < 0
    similarity[threshold] = 0

    # calculate weight
    weights = np.exp(-similarity / h**2)

    # compute contributions
    neighbor_pixels = neighbor_patch[:, int(neighbor_patch.shape[1]/2)]
    contributions = np.multiply(weights, neighbor_pixels)

    # return the weight and the contribution it would make to denoising
    return weights, contributions

# Non Local Mean filter
# n_large: search square window size of neighboring pixel j
# n_large: square patch size
# h: degree of filtering
# a: Gaussian standard deviation
def nonlocalmeans_2D(in_image, n_large, n_small, h, a):

    if (len(in_image.shape) != 2):
        print("Error: input slice image dimension is not 2D")
        return in_image

    # initialize output image
    out_image = np.zeros_like(in_image)

    # some info about input image
    num_rows = in_image.shape[0]
    num_columns = in_image.shape[1]
    print("in_image has " + str(num_rows) + " rows, " + str(num_columns) + " columns")

    # iterate through all pixels, j is rows, i is columns
    for j in range(num_rows):
        for i in range(num_columns):
            # progress monitor
            if (j%20 == 0 and i == 0):
                print("row = " + str(j) + ", column = " + str(i))

            # normalization factor
            Z = 0
            # all the nearby pixels (y, x) within n_large, y is rows, x is columns
            for y in range(-n_large+j, n_large+1+j):
                for x in range(-n_large+i, n_large+1+i):
                    # make sure the patch center is within image bound
                    if (x >= 0 and x < num_columns and y >= 0 and y < num_rows):
                        cur_weight = calculate_weight(in_image, i, j, x, y, n_small, h, a)
                        Z += cur_weight
                        out_image[j][i] += cur_weight * in_image[y][x]

            out_image[j][i] /= Z

    return out_image

# optimized method
# @profile
def nonlocalmeans_2D_fast(in_image, n_large, n_small, h, a):
    from functools import reduce, partial
    import operator

    if (len(in_image.shape) != 2):
        print("Error: input slice image dimension is not 2D")
        return in_image

    # initialize output image
    out_image = np.zeros_like(in_image)

    # some info about input image
    num_rows = in_image.shape[0]
    num_columns = in_image.shape[1]
    print("in_image has " + str(num_rows) + " rows, " + str(num_columns) + " columns")

    # precompute coordinate difference for the neighbors
    neighbor_range = range(-n_large, n_large + 1)
    # exclude the (0, 0) point which would be the original point
    neighbor_window = [(row, column) for row in neighbor_range for column in neighbor_range if not (row == 0 and column == 0)]

    # precompute coordinate difference for the individual patch that would be used to calculate weight
    patch_rows, patch_cols = np.indices((2*n_small+1, 2*n_small+1))-n_small

    # padding for denoising "corner" pixels
    padding_length = n_large + n_small
    padded_image = np.pad(in_image, padding_length, mode='wrap')

    # iterate through all pixels, j is rows, i is columns
    # offset is the padding_length so we are still iterating within in_image
    for j in range(padding_length, num_rows + padding_length):
        for i in range(padding_length, num_columns + padding_length):
            # progress monitor
            if ((j-padding_length)%1 == 0 and (i-padding_length) == 0):
                print("row = " + str(j-padding_length) + ", column = " + str(i-padding_length))

            # patch around the current pixel
            pixel_patch_values = padded_image[j+patch_rows, i+patch_cols].flatten()

            # patches around every neighbor location of the current pixel
            neighbors_patch_values = [padded_image[j+neighbor[0]+patch_rows, i+neighbor[1]+patch_cols].flatten() for neighbor in neighbor_window]

            # calculate the weights
            weight_map = partial(calculate_weight_fast, pixel_patch=pixel_patch_values, n_small=n_small, h=h, a=a)
            weights = map(weight_map, zip(neighbors_patch_values))

            # Z is the sum of all weights, C is the sum of weight*neighbor_pixel, as indicated in paper
            Z, C  = reduce(lambda a, b: (a[0] + b[0], a[1] + b[1]), weights)
            out_image[j-padding_length, i-padding_length] = C / Z

    return out_image

# optimized method 2
# @profile
def nonlocalmeans_2D_fast2(in_image, n_large, n_small, h, a):

    if (len(in_image.shape) != 2):
        print("Error: input slice image dimension is not 2D")
        return in_image

    # initialize output image
    out_image = np.zeros_like(in_image)

    # some info about input image
    num_rows = in_image.shape[0]
    num_columns = in_image.shape[1]
    print("in_image has " + str(num_rows) + " rows, " + str(num_columns) + " columns")

    # precompute coordinate difference for the neighbors
    neighbor_range = range(-n_large, n_large + 1)
    # exclude the (0, 0) point which would be the original point
    neighbor_window = [(row, column) for row in neighbor_range for column in neighbor_range if not (row == 0 and column == 0)]

    # precompute coordinate difference for the individual patch that would be used to calculate weight
    patch_rows, patch_cols = np.indices((2*n_small+1, 2*n_small+1))-n_small

    # padding for denoising "corner" pixels
    padding_length = n_large + n_small
    padded_image = np.pad(in_image, padding_length, mode='wrap')

    # iterate through all pixels, j is rows, i is columns
    # offset is the padding_length so we are still iterating within in_image
    for j in range(padding_length, num_rows + padding_length):
        for i in range(padding_length, num_columns + padding_length):
            # progress monitor
            if ((j-padding_length)%1 == 0 and (i-padding_length) == 0):
                print("row = " + str(j-padding_length) + ", column = " + str(i-padding_length))

            # patch around the current pixel, flattened to 1D
            pixel_patch_values = padded_image[j+patch_rows, i+patch_cols].flatten()
            # repeat the same row num_neighbor times
            pixel_patch_values = np.array([pixel_patch_values,]*len(neighbor_window))

            # patches around every neighbor location of the current pixel
            neighbors_patch_values = np.array([padded_image[j+neighbor[0]+patch_rows, i+neighbor[1]+patch_cols].flatten() for neighbor in neighbor_window])

            # calculate the weights
            weights, contributions = calculate_weight_fast2(pixel_patch_values, neighbors_patch_values, n_small, h, a)

            # Z is the sum of all weights, C is the sum of weight*neighbor_pixel, as indicated in paper
            Z = np.sum(weights)
            C = np.sum(contributions)
            out_image[j-padding_length, i-padding_length] = C / Z

    return out_image


@profile
def nonlocalmeans_2D_fast3(in_image, n_small, num_features, num_neighbors, h, a):
    import scipy as sp
    from sklearn.decomposition import PCA
    # from sklearn.neighbors.ball_tree import BallTree

    if (len(in_image.shape) != 2):
        print("Error: input slice image dimension is not 2D")
        return in_image

    # initialize output image
    out_image = np.zeros_like(in_image)

    # some info about input image
    num_rows = in_image.shape[0]
    num_columns = in_image.shape[1]
    print("in_image has " + str(num_rows) + " rows, " + str(num_columns) + " columns")

    # precompute coordinate difference for the individual patch that would be used to calculate weight
    patch_rows, patch_cols = np.indices((2*n_small+1, 2*n_small+1))-n_small

    # padding for denoising "corner" pixels
    padding_length = n_small
    padded_image = np.pad(in_image, padding_length, mode='wrap')

    all_patches = np.zeros((num_rows*num_columns, (2*n_small+1)**2))

    for j in range(n_small, n_small + num_rows):
        for i in range(n_small, n_small + num_columns):
            pixel_patch_values = padded_image[j+patch_rows, i+patch_cols].flatten()
            all_patches[(j-n_small)*num_rows+(i-n_small), :] = pixel_patch_values

    all_patches_transformed = PCA(n_components=num_features).fit_transform(all_patches)
    # index the patches into a tree so we can get the nearest neighbors fast
    neighbors_tree = sp.spatial.cKDTree(all_patches_transformed)

    # iterate through all pixels, j is rows, i is columns
    for j in range(num_rows):
        for i in range(num_columns):
            # progress monitor
            if (j%1 == 0 and i == 0):
                print("row = " + str(j) + ", column = " + str(i))

            # patch around the current pixel
            pixel_index = j * num_rows + i
            pixel_patch_values = all_patches_transformed[pixel_index]
            # repeat the same row num_neighbor times
            pixel_patch_values = np.array([pixel_patch_values,]*num_neighbors)

            # patches around every neighbor location of the current pixel
            _, neighbors_patch_coordinates = neighbors_tree.query(pixel_patch_values, k=num_neighbors)
            neighbors_patch_values = np.array(all_patches_transformed[neighbors_patch_coordinates])

            # calculate the weights
            weights, contributions = calculate_weight_fast2(pixel_patch_values, neighbors_patch_values, n_small, h, a)

            # Z is the sum of all weights, C is the sum of weight*neighbor_pixel, as indicated in paper
            Z = np.sum(weights)
            C = np.sum(contributions)
            out_image[j, i] = C / Z

    return out_image

# this is for use in Tomviz
# def transform_scalars(dataset):
#     """
#     Define this method for Python operators that transform the input array
#     """

#     # Get the current volume as a numpy array.
#     array = utils.get_array(dataset)
#     new_array = np.zeros_like(array)

#     print('Input data has shape = ' + str(array.shape))
#     # perform each slice individually when it is a 3D volume
#     if (len(array.shape) == 3):
#         print("Performing NL-means denoising slice by slice")
#         for i in range(array.shape[0]):
#             print("Denoising slice " + str(i))
#             cur_slice = array[i, :, :]
#             cur_result = nonlocalmeans_2D(cur_slice, n_large=9, n_small=5, h=1, a=1)
#             new_array[i, :, :] = cur_result

#     # This is where the transformed data is set, it will display in tomviz.
#     utils.set_array(dataset, new_array)

def add_gaussian_noise(in_image, sigma):
    row, col = in_image.shape
    mean = 0
    gauss = np.random.normal(mean, sigma, (row, col))
    gauss = gauss.reshape(row, col)
    out_image = in_image + gauss

    return out_image

if __name__ == "__main__":
    import time
    from PIL import Image
    from skimage import data, img_as_float
    from skimage.restoration import denoise_nl_means, estimate_sigma
    from skimage.measure import compare_psnr
    from skimage.util import random_noise

    # read input image and convert to greyscale
    in_image = Image.open('/home/zhuokai/Desktop/Lenna.png').convert('L')
    # resize image to smaller size for less computation time
    resize_image = False
    if (resize_image):
        basewidth = 64
        wpercent = (basewidth / float(in_image.size[0]))
        hsize = int((float(in_image.size[1]) * float(wpercent)))
        in_image = in_image.resize((basewidth, hsize), Image.ANTIALIAS)
        # in_image.show()
        if in_image.mode != 'RGB':
            in_image_new = in_image.convert('RGB')
        in_image_new.save('/home/zhuokai/Desktop/Lenna_resized.png', 'PNG')

    # add noise
    in_image_np = np.array(in_image)
    noised_image_np = add_gaussian_noise(in_image_np, 10)
    noised_image = Image.fromarray(noised_image_np)
    # noised_image.show()
    if noised_image.mode != 'RGB':
        noised_image = noised_image.convert('RGB')
    noised_image.save('/home/zhuokai/Desktop/Lenna_noised.png', 'PNG')

    # NLM denoising using my implementation
    start_time1 = time.time()
    # denoised_image_np = nonlocalmeans_2D_fast2(noised_image_np, n_large=5, n_small=3, h=10, a=1)
    denoised_image_np = nonlocalmeans_2D_fast3(noised_image_np, n_small=3, num_features=25, num_neighbors=5, h=10, a=1)
    denoised_image = Image.fromarray(denoised_image_np)
    # denoised_image.show()
    if denoised_image.mode != 'RGB':
        denoised_image = denoised_image.convert('RGB')
    denoised_image.save('/home/zhuokai/Desktop/Lenna_denoised.png', 'PNG')
    print("My implementation took %s seconds ---" % (time.time() - start_time1))

    # NLM denoising using scikit-image
    # estimate the noise standard deviation from the noisy image
    # sigma_est = np.mean(estimate_sigma(noisy, multichannel=True))
    # print("estimated noise standard deviation = {}".format(sigma_est))
    start_time2 = time.time()
    patch_kw = dict(patch_size=5,      # 5x5 patches
                    patch_distance=6,  # 13x13 search area
                    multichannel=False)

    # slow algorithm
    denoised_image_sk_np = denoise_nl_means(noised_image_np, h=10, fast_mode=False,
                                **patch_kw)
    denoised_image_sk = Image.fromarray(denoised_image_sk_np)
    if denoised_image_sk.mode != 'RGB':
        denoised_image_sk = denoised_image_sk.convert('RGB')
    denoised_image_sk.save('/home/zhuokai/Desktop/Lenna_denoised_sk.png', 'PNG')
    print("SK implementation took %s seconds ---" % (time.time() - start_time2))

    # # slow algorithm, sigma provided
    # denoise2 = denoise_nl_means(noisy, h=0.8 * sigma_est, sigma=sigma_est,
    #                             fast_mode=False, **patch_kw)

    # # fast algorithm
    # denoise_fast = denoise_nl_means(noisy, h=0.8 * sigma_est, fast_mode=True,
    #                                 **patch_kw)

    # # fast algorithm, sigma provided
    # denoise2_fast = denoise_nl_means(noisy, h=0.6 * sigma_est, sigma=sigma_est,
    #                                 fast_mode=True, **patch_kw)

    # compute MSE of my implementation
    noised_MSE = compute_MSE(in_image_np, noised_image_np)
    denoised_MSE = compute_MSE(in_image_np, denoised_image_np)
    denoised_MSE_sk = compute_MSE(in_image_np, denoised_image_sk_np)
    print('MSE between original and noised:', noised_MSE)
    print('MSE between original and my denoised:', denoised_MSE)
    print('MSE between original and sk denoised:', denoised_MSE_sk)

    # compute PSNR of my implementation
    noised_PSNR = compute_PSNR(in_image_np, noised_image_np)
    denoised_PSNR = compute_PSNR(in_image_np, denoised_image_np)
    denoised_PSNR_sk = compute_PSNR(in_image_np, denoised_image_sk_np)
    print('PSNR between original and noised:', noised_PSNR)
    print('PSNR between original and my denoised:', denoised_PSNR)
    print('PSNR between original and sk denoised:', denoised_PSNR_sk)

