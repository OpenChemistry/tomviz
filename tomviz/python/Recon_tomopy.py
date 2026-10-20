import ctypes
import os

import numpy as np
import tomopy


def _cuda_device_count(accel_lib: ctypes.CDLL) -> int:
    # TomoPy's own count, which its Linux builds export
    if hasattr(accel_lib, 'cuda_device_count'):
        return accel_lib.cuda_device_count()

    # Windows builds export only cxx_sirt and cxx_mlem, so ask the CUDA
    # driver (this misses a driver too old for TomoPy's CUDA version)
    try:
        cuda = ctypes.CDLL('nvcuda.dll' if os.name == 'nt' else 'libcuda.so.1')
    except OSError:
        return 0
    count = ctypes.c_int(0)
    if cuda.cuInit(0) != 0:
        return 0
    if cuda.cuDeviceGetCount(ctypes.byref(count)) != 0:
        return 0
    return count.value


def _check_gpu_available() -> None:
    # TomoPy aborts the whole process (an uncaught C++ exception) when it
    # finds no CUDA device, so check before using its accelerated path
    from tomopy.util.extern import accel
    if accel.LIB_TOMOPY_ACCEL is None:
        raise RuntimeError(
            'GPU reconstruction is not available in this installation: '
            'TomoPy was installed without its CUDA library (the CUDA build '
            'of libtomo). Uncheck "Use GPU (CUDA)" to reconstruct on the '
            'CPU.')
    if _cuda_device_count(accel.LIB_TOMOPY_ACCEL) < 1:
        raise RuntimeError(
            'GPU reconstruction requires an NVIDIA GPU with a working CUDA '
            'driver, and none was found. Uncheck "Use GPU (CUDA)" to '
            'reconstruct on the CPU.')


def transform(dataset, algorithm='gridrec', num_iter=5, reg_par=0.1,
              use_gpu=False):
    data = dataset.active_scalars
    tilt_axis = dataset.tilt_axis

    # TomoPy wants the tilt axis to be zero, so ensure that is true
    if tilt_axis == 2:
        data = np.ascontiguousarray(np.transpose(data, (2, 0, 1)))

    # Normalize to [0, 1]
    data = data.astype(np.float32)
    data = (data - data.min()) / (data.max() - data.min())

    angles_rad = np.deg2rad(dataset.tilt_angles)
    center = data.shape[2] // 2

    # Reconstruct
    recon_kwargs = {}
    if algorithm in ('sirt', 'art', 'tv', 'mlem', 'ospml_hybrid'):
        recon_kwargs['num_iter'] = num_iter
    if algorithm == 'tv':
        recon_kwargs['reg_par'] = reg_par
    if use_gpu and algorithm in ('sirt', 'mlem'):
        _check_gpu_available()
        recon_kwargs['accelerated'] = True
        recon_kwargs['device'] = 'gpu'
        # Without ncore, TomoPy splits the slices across one process per
        # CPU core, each creating its own CUDA context on the same GPU,
        # which makes the GPU path much slower than the CPU one.
        recon_kwargs['ncore'] = 1

    rec = tomopy.recon(data, angles_rad, center=center, algorithm=algorithm,
                       **recon_kwargs)

    # Apply circular mask
    rec = tomopy.circ_mask(rec, axis=0, ratio=0.95, val=0.0)

    # TomoPy already returns the rotation axis along axis 0 (X), matching
    # the (Nslice, N, N) convention of the other Tomviz recon operators.
    child = dataset.create_child_dataset()
    child.active_scalars = rec

    return {'reconstruction': child}
