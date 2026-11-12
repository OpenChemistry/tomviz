import ctypes
import os

import numexpr
import numpy as np
import tomopy

import tomviz.operators
from tomviz.dataset import Dataset


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


class TomoPyReconOperator(tomviz.operators.CancelableOperator):

    def transform(self, dataset: Dataset, algorithm: str = 'gridrec',
                  num_iter: int = 5, reg_par: float = 0.1,
                  use_gpu: bool = False) -> dict[str, Dataset] | None:
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
            # TomoPy otherwise starts a GPU thread per CPU core, and with
            # hundreds of cores the threads starve the GPU. Eight was the
            # fastest in our tests.
            recon_kwargs['pool_size'] = 8

        # Reconstruct a few slices at a time so it can be canceled. Each
        # slice is independent, and TomoPy splits them across cores anyway.
        # The GPU is only efficient with many slices per call.
        num_slices, width = data.shape[1], data.shape[2]
        gpu = recon_kwargs.get('accelerated', False)
        step = max(128 if gpu else (os.cpu_count() or 1), num_slices // 50)
        self.progress.maximum = num_slices
        rec = np.empty((num_slices, width, width), dtype=np.float32)
        start = 0
        while start < num_slices:
            if self.canceled:
                return
            stop = min(start + step, num_slices)
            self.progress.message = (
                f'Reconstructing slices {start + 1}-{stop} of {num_slices}')
            chunk = tomopy.recon(data[:, start:stop], angles_rad,
                                 center=center, algorithm=algorithm,
                                 **recon_kwargs)
            # Out of GPU memory, TomoPy prints an error and returns its
            # starting guess (1e-6 everywhere), so retry with fewer slices
            if gpu and np.all(chunk == np.float32(1e-6)):
                if stop - start == 1:
                    raise RuntimeError('Not enough GPU memory for one slice')
                step = (stop - start) // 2
                continue
            rec[start:stop] = chunk
            self.progress.value = stop
            start = stop

        # Apply circular mask. TomoPy asks numexpr for a thread per core,
        # which prints errors past numexpr's limit (64), so keep its count.
        rec = tomopy.circ_mask(rec, axis=0, ratio=0.95, val=0.0,
                               ncore=numexpr.get_num_threads())

        # TomoPy already returns the rotation axis along axis 0 (X), matching
        # the (Nslice, N, N) convention of the other Tomviz recon operators.
        child = dataset.create_child_dataset()
        child.active_scalars = rec

        return {'reconstruction': child}
