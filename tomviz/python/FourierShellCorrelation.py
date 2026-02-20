import tomviz.operators
import tomviz.utils

import numpy as np
import scipy.stats as stats

# Fourier Shell correlation, Xiaozing's Code. 

def cal_dist(shape):
    if np.size(shape) == 2:
        nx,ny = shape
        dist_map = np.zeros((nx,ny))
        for i in range(nx):
            for j in range(ny):
                dist_map[i,j] = np.sqrt((i-nx/2)**2+(j-ny/2)**2)

    elif np.size(shape) == 3:
        nx,ny,nz = shape
        dist_map = np.zeros((nx,ny,nz))
        for i in range(nx):
            for j in range(ny):
                for k in range(nz):
                    dist_map[i,j,k] = np.sqrt((i-nx/2)**2+(j-ny/2)**2+(k-nz/2)**2)
    
    else:
        raise ValueError("Wrong image dimensions.")

    return dist_map


def cal_fsc(image1,image2,pixel_size_nm,phase_flag=False, save=False, title=None):

    if np.ndim(image1) == 2:
        if phase_flag:
            image1 = np.angle(image1)
            image2 = np.angle(image2)
        nx,ny = np.shape(image1)
        image1_fft = np.fft.fftshift(np.fft.fftn(np.fft.fftshift(image1))) / np.sqrt(nx*ny*1.)
        image2_fft = np.fft.fftshift(np.fft.fftn(np.fft.fftshift(image2))) / np.sqrt(nx*ny*1.)
        #r_max = int(np.sqrt(nx**2/4.+ny**2/4.))
        r_max = int(np.max((nx,ny))/2)
        fsc = np.zeros(r_max)
        noise_onebit = np.zeros(r_max)
        noise_halfbit = np.zeros(r_max)
        x = np.arange(nx/2)/(nx*pixel_size_nm)
        #x = np.arange(r_max)/(nx*pixel_size_nm)
        dist_map = cal_dist((nx,ny))
        #np.save('dist_map.np',dist_map)
        for i in range(r_max):
            index = np.where((i <= dist_map) & (dist_map < (i+1)))
            fsc[i] = np.abs(np.sum(image1_fft[index] * np.conj(image2_fft[index])) / 
                             np.sqrt(np.sum(np.abs(image1_fft[index])**2)*np.sum(np.abs(image2_fft[index])**2)))
            n_point = np.size(index) / (2)
            if n_point > 0:
                noise_onebit[i] = (0.5+2.4142/np.sqrt(n_point)) / (1.5+1.4142/np.sqrt(n_point))
                noise_halfbit[i] = (0.2071+1.9102/np.sqrt(n_point)) / (1.2071+0.9102/np.sqrt(n_point))

    elif np.ndim(image1) == 3:
        if phase_flag:
            image1 = np.angle(image1)
            image2 = np.angle(image2)
        nx,ny,nz = np.shape(image1)
        image1_fft = np.fft.fftshift(np.fft.fftn(np.fft.fftshift(image1))) / np.sqrt(nx*ny*nz*1.)
        image2_fft = np.fft.fftshift(np.fft.fftn(np.fft.fftshift(image2))) / np.sqrt(nx*ny*nz*1.)
        #r_max = int(np.sqrt(nx**2/4+ny**2/4+nz**2/4))
        r_max = int(np.max((nx,ny,nz))/2)
        fsc = np.zeros(r_max)
        noise_onebit = np.zeros(r_max)
        noise_halfbit = np.zeros(r_max)
        x = np.arange(nx/2)/(nx*pixel_size_nm)
        dist_map = cal_dist((nx,ny,nz))
        for i in range(r_max):
            index = np.where((i <= dist_map) & (dist_map < (i+1)))
            fsc[i] = np.abs(np.sum(image1_fft[index] * np.conj(image2_fft[index])) / np.sqrt(np.sum(np.abs(image1_fft[index])**2)*np.sum(np.abs(image2_fft[index])**2)))
            n_point = np.size(index) / (3)
            if n_point > 0:
                noise_onebit[i] = (0.5+2.4142/np.sqrt(n_point)) / (1.5+1.4142/np.sqrt(n_point))
                noise_halfbit[i] = (0.2071+1.9102/np.sqrt(n_point)) / (1.2071+0.9102/np.sqrt(n_point))
    
    else:
        raise ValueError("Wrong image dimensions.")

    return x, fsc, noise_onebit, noise_halfbit


def checkerboard_split(image):
    shape = image.shape
    odd_index = list(np.arange(1, shape[i], 2) for i in range(len(shape)))
    even_index = list(np.arange(0, shape[i], 2) for i in range(len(shape)))
    image1 = image[even_index[0], :, :][:, odd_index[1], :][:, :, odd_index[2]] + \
                     image[odd_index[0], :, :][:, odd_index[1], :][:, :, odd_index[2]]

    image2 = image[even_index[0], :, :][:, even_index[1], :][:, :, even_index[2]] + \
             image[odd_index[0], :, :][:, even_index[1], :][:, :, even_index[2]]

    return image1, image2


class FourierShellCorrelation(tomviz.operators.CancelableOperator):
    def transform(self, dataset):
        scalars = dataset.active_scalars
        pixel_spacing = dataset.spacing[0]

        if scalars is None:
            raise RuntimeError("No scalars found!")

        image1, image2 = checkerboard_split(scalars)
        x, fsc, noise_onebit, noise_halfbit = cal_fsc(image1, image2, pixel_spacing)

        column_names = ["x", "FSC", "One bit noise", "Half bit noise"]

        n = len(x)

        table_data = np.empty(shape=(n, 4))

        table_data[:, 0] = x
        table_data[:, 1] = fsc
        table_data[:, 2] = noise_onebit
        table_data[:, 3] = noise_halfbit

        table = tomviz.utils.make_spreadsheet(column_names, table_data, ("Spatial Frequency", "Fourier Shell Correlation"), (False, False))

        return_values = {
            "plot": table
        }

        return return_values
