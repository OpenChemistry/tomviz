import numpy as np
import pytest

from utils import load_operator_class, load_operator_module

from tomviz_pipeline.dataset import LegacyDataset as Dataset

try:
    import tomopy
    HAS_TOMOPY = True
except ImportError:
    HAS_TOMOPY = False


@pytest.mark.skipif(not HAS_TOMOPY, reason="tomopy not installed")
def test_tomopy_reconstruction():
    """Test basic tomopy reconstruction via rotcen_test."""
    module = load_operator_module('ShiftRotationCenter_tomopy')

    # Create a simple synthetic sinogram
    num_angles = 30
    detector_width = 32
    theta = np.linspace(0, np.pi, num_angles, endpoint=False)

    # Simple phantom: a circle
    img_tomo = np.zeros((num_angles, 1, detector_width), dtype=np.float32)
    for i in range(num_angles):
        center = detector_width // 2
        for j in range(detector_width):
            if abs(j - center) < detector_width // 4:
                img_tomo[i, 0, j] = 1.0

    recon_input = {
        'img_tomo': img_tomo,
        'angle': np.degrees(theta),
    }

    images, centers = module.rotcen_test(
        f=recon_input,
        start=-3,
        stop=3,
        steps=5,
        sli=0,
        algorithm='gridrec',
    )

    # Verify output shape: steps images, each detector_width x detector_width
    assert images.shape[0] == 5
    assert images.shape[1] == detector_width
    assert images.shape[2] == detector_width

    # Verify reconstruction is not all zeros
    assert not np.allclose(images, 0)

    # Verify centers length matches steps
    assert len(centers) == 5


@pytest.mark.skipif(not HAS_TOMOPY, reason="tomopy not installed")
def test_tomopy_reconstruction_different_algorithms():
    """Test reconstruction with gridrec and fbp algorithms."""
    module = load_operator_module('ShiftRotationCenter_tomopy')

    num_angles = 30
    detector_width = 32
    theta = np.linspace(0, np.pi, num_angles, endpoint=False)

    img_tomo = np.zeros((num_angles, 1, detector_width), dtype=np.float32)
    for i in range(num_angles):
        center = detector_width // 2
        for j in range(detector_width):
            if abs(j - center) < detector_width // 4:
                img_tomo[i, 0, j] = 1.0

    recon_input = {
        'img_tomo': img_tomo,
        'angle': np.degrees(theta),
    }

    for algorithm in ('gridrec', 'fbp'):
        images, centers = module.rotcen_test(
            f=recon_input,
            start=-2,
            stop=2,
            steps=3,
            sli=0,
            algorithm=algorithm,
        )
        assert images.shape[0] == 3, f"Failed for algorithm {algorithm}"
        assert not np.allclose(images, 0), f"All zeros for algorithm {algorithm}"


@pytest.mark.skipif(not HAS_TOMOPY, reason="tomopy not installed")
def test_tomopy_gpu_check_raises_instead_of_aborting(
        monkeypatch: pytest.MonkeyPatch) -> None:
    from types import SimpleNamespace
    from tomopy.util.extern import accel
    module = load_operator_module('Recon_tomopy')

    monkeypatch.setattr(accel, 'LIB_TOMOPY_ACCEL', None)
    with pytest.raises(RuntimeError, match='CUDA library'):
        module._check_gpu_available()

    no_devices = SimpleNamespace(cuda_device_count=lambda: 0)
    monkeypatch.setattr(accel, 'LIB_TOMOPY_ACCEL', no_devices)
    with pytest.raises(RuntimeError, match='NVIDIA GPU'):
        module._check_gpu_available()


def _tilt_series() -> Dataset:
    # A bar in each of 12 slices, seen from 30 angles
    angles = np.linspace(-70, 70, 30)
    data = np.zeros((12, 32, len(angles)), dtype=np.float32)
    data[:, 12:20, :] = 1.0
    dataset = Dataset({'intensity': data})
    dataset.tilt_angles = angles
    dataset.tilt_axis = 2
    return dataset


@pytest.mark.skipif(not HAS_TOMOPY, reason="tomopy not installed")
def test_tomopy_recon_in_chunks_matches_one_call(
        monkeypatch: pytest.MonkeyPatch) -> None:
    operator = load_operator_class(load_operator_module('Recon_tomopy'))
    # Two slices per chunk
    monkeypatch.setattr('os.cpu_count', lambda: 2)
    rec = operator.transform(_tilt_series(), algorithm='mlem',
                             num_iter=3)['reconstruction'].active_scalars

    data = np.transpose(_tilt_series().active_scalars, (2, 0, 1))
    data = (data - data.min()) / (data.max() - data.min())
    expected = tomopy.recon(np.ascontiguousarray(data),
                            np.deg2rad(np.linspace(-70, 70, 30)),
                            center=16, algorithm='mlem', num_iter=3)
    expected = tomopy.circ_mask(expected, axis=0, ratio=0.95, val=0.0)
    assert np.allclose(rec, expected)


@pytest.mark.skipif(not HAS_TOMOPY, reason="tomopy not installed")
def test_tomopy_recon_cancel() -> None:
    operator = load_operator_class(load_operator_module('Recon_tomopy'))
    operator._operator_wrapper._canceled = True
    assert operator.transform(_tilt_series(), algorithm='mlem') is None
