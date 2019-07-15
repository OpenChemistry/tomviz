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
    # each weight corresponds to each neighbor pixel
    # weights = np.zeros((2*n_large+1, 2*n_large+1))

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
                        # print('current neighbor pixel ', y, x)
                        # weights[y+n_large-j][x+n_large-i] = calculate_weight(in_image, i, j, x, y, n_small, h, a)
                        # Z += weights[y+n_large-j][x+n_large-i]
                        # out_image[j][i] += weights[y+n_large-j][x+n_large-i] * in_image[y][x]
                        cur_weight = calculate_weight(in_image, i, j, x, y, n_small, h, a)
                        # print('current weight ', cur_weight)
                        Z += cur_weight
                        out_image[j][i] += cur_weight * in_image[y][x]

            # print('Z', Z)
            out_image[j][i] /= Z


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

def main():
    import numpy as np
    from PIL import Image


    # read input image and convert to greyscale
    in_image = Image.open('/home/zhuokai/Desktop/Lenna.png').convert('L')
    # resize image to smaller size for less computation time
    resize_image = True
    if (resize_image):
        basewidth = 128
        wpercent = (basewidth / float(in_image.size[0]))
        hsize = int((float(in_image.size[1]) * float(wpercent)))
        in_image = in_image.resize((basewidth, hsize), Image.ANTIALIAS)
        # in_image.show()

    # add noise
    in_image_np = np.array(in_image)
    noised_image_np = add_gaussian_noise(in_image_np, 10)
    noised_image = Image.fromarray(noised_image_np)
    # noised_image.show()
    if noised_image.mode != 'RGB':
        noised_image = noised_image.convert('RGB')
    noised_image.save('/home/zhuokai/Desktop/Lenna_noised.png', 'PNG')

    # NLM denoising
    denoised_image_np = nonlocalmeans_2D(noised_image_np, n_large=9, n_small=5, h=10, a=1)
    denoised_image = Image.fromarray(denoised_image_np)
    # denoised_image.show()
    if denoised_image.mode != 'RGB':
        denoised_image = noised_image.convert('RGB')
    denoised_image.save('/home/zhuokai/Desktop/Lenna_denoised.png', 'PNG')

    # compute MSE
    noised_MSE = compute_MSE(in_image_np, noised_image_np)
    denoised_MSE = compute_MSE(in_image_np, denoised_image_np)
    print('MSE between original and noised:', noised_MSE)
    print('MSE between original and denoised:', denoised_MSE)

    # compute PSNR
    noised_PSNR = compute_PSNR(in_image_np, noised_image_np)
    denoised_PSNR = compute_PSNR(in_image_np, denoised_image_np)
    print('PSNR between original and noised:', noised_PSNR)
    print('PSNR between original and denoised:', denoised_PSNR)


if __name__ == "__main__":
    import time
    start_time = time.time()
    main()
    print("--- %s seconds ---" % (time.time() - start_time))