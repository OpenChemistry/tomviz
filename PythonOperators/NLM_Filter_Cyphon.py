import numpy as np
import _NLM_Filter
import time
from PIL import Image
from skimage import data, img_as_float
from skimage.restoration import denoise_nl_means, estimate_sigma
from skimage.measure import compare_psnr
from skimage.util import random_noise


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

def add_gaussian_noise(in_image, sigma):
    row, col = in_image.shape
    mean = 0
    gauss = np.random.normal(mean, sigma, (row, col))
    gauss = gauss.reshape(row, col)
    out_image = in_image + gauss
    return out_image

# @profile
def main():
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
    denoised_image_np = _NLM_Filter._nonlocalmeans_2D_fast(noised_image_np, n_large=5, n_small=3, h=10, a=1)
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

if __name__ == "__main__":
    main()
