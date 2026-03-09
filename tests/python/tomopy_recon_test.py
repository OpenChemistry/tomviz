import numpy as np
import pytest

from utils import load_operator_module

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
