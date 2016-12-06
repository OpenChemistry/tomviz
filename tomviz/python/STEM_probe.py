

def generate_dataset(array, voltage=300.0, alpha_max=30.0, Nxy=256, Nz=512,
                     dxy=0.1, df_min=-50.0, df_max=100.0, c3=0.20, f_a2=0.0,
                     phi_a2=0.0, f_a3=0.0, phi_a3=0.0, f_c3=1500.0, phi_c3=0.0):
    """Generate STEM probe function"""

    import numpy as np

    #---------------------------------#
    #Convert all units to angstrom
    c3 = c3 * 1e7
    f_a2 = f_a2 * 10
    f_a3 = f_a3 * 10
    f_c3 = f_c3 * 10
    df_min = df_min * 10
    df_max = df_max * 10

    wavelength = 12.398 / np.sqrt((2 * 511.0 + voltage) * voltage)  #angstrom
    k_max = alpha_max * 1e-3 / wavelength
    k_min = 0.0
    dk = 1.0 / (dxy * Nxy)

    kx = np.linspace(-np.floor(Nxy / 2.0), np.ceil(Nxy / 2.0) - 1, Nxy)
    [kY, kX] = np.meshgrid(kx, kx)
    kX = kX * dk
    kY = kY * dk
    kR = np.sqrt(kX**2 + kY**2)
    phi = np.arctan2(kY, kX)
    df = np.linspace(df_min, df_max, Nz)

    for i in range(0, Nz):
        defocus = df[i]
        chi = (-np.pi * wavelength * kR**2 * defocus + np.pi / 2 * c3 *
               wavelength**3 * kR**4 + np.pi * f_a2 * wavelength * kR**2 *
               np.sin(2 * (phi - phi_a2)) + f_a3 * wavelength**2 * kR**3 *
               np.sin(3 * (phi - phi_a3)) + 2 * np.pi / 3 * f_c3 *
               wavelength**2 * kR**3 * np.sin(phi - phi_c3))

        probe = np.exp(-1j * chi)
        probe[kR > k_max] = 0
        probe[kR < k_min] = 0

        probe = np.fft.fftshift(np.fft.ifft2(np.fft.ifftshift(probe)))
        probe = probe / np.sqrt(np.sum(np.abs(probe)**2) * dxy * dxy)

        np.copyto(array[:, :, i], np.abs(probe))
