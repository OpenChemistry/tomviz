import numpy as np
import pytest

from utils import load_operator_class, load_operator_module

from tomviz_pipeline.dataset import LegacyDataset as Dataset

METHODS = ('landweber', 'cimmino', 'component averaging')


class Progress:
    maximum = 0
    value = 0
    message = ''
    data = None


def phantom(n: int, nslice: int) -> np.ndarray:
    y, x = np.mgrid[:n, :n] - (n - 1) / 2
    image = ((x / (0.4 * n)) ** 2 + (y / (0.3 * n)) ** 2 <= 1) * 1.0
    image += ((x - 0.1 * n) ** 2 + y ** 2 <= (0.08 * n) ** 2) * 0.5
    return np.repeat(image[None].astype(np.float32), nslice, 0)


def reconstruct(n: int, nproj: int, **kwargs) -> float:
    """RMSE of a SIRT reconstruction of a phantom from exact projections."""
    module = load_operator_module('Recon_SIRT')
    angles = np.linspace(-90, 90, nproj, endpoint=False)
    # The operator shifts the angles by 0.001 when one is zero
    A = module.parallelRay(n, 1.0, angles + 0.001, n, 1.0).tocsr()
    truth = phantom(n, 2)
    # Tilt series (Nslice, Nray, Nproj), as the operator reads it
    tilt = np.stack([(A @ s.ravel()).reshape(nproj, n).T for s in truth])
    dataset = Dataset({'scalars': np.asfortranarray(tilt)})
    dataset.spacing = [1.0, 1.0, 1.0]
    dataset.tilt_angles = angles
    operator = load_operator_class(module)
    operator.progress = Progress()
    out = operator.transform(dataset, Niter=10, Nupdates=0, **kwargs)
    recon = out['reconstruction'].active_scalars
    return float(np.sqrt(np.mean((recon - truth) ** 2)))


@pytest.mark.parametrize('method', METHODS)
def test_automatic_step_is_one_over_the_largest_eigenvalue(method: str):
    module = load_operator_module('Recon_SIRT')
    angles = np.linspace(-90, 90, 30, endpoint=False) + 0.001
    A = module.parallelRay(16, 1.0, angles, 16, 1.0)
    sirt = module.SIRT(A, method, 1)
    sirt.initialize()
    # The update operator as a dense matrix, from the update itself: with
    # no data and step 1, recon2 maps x to x - B x
    b = np.zeros(A.shape[0], dtype=np.float32)
    B = np.column_stack([e - sirt.recon2(b, e.astype(np.float32), 1.0, 0)
                         for e in np.eye(A.shape[1])])
    rho = np.linalg.eigvalsh((B + B.T) / 2).max()
    assert sirt.automatic_step() == pytest.approx(1 / rho, rel=1e-3)


def test_default_step_converges_where_the_old_default_diverges():
    # 64 pixels from 360 projections: ||A||^2 is about 22000, so Landweber
    # needs a step below 2 / ||A||^2 = 9e-5, and the old default 1e-4
    # diverges. The positivity constraint hides it: the volume stays
    # bounded, but far from the phantom.
    old = reconstruct(64, 360, stepSize=1e-4, updateMethodIndex=0)
    new = reconstruct(64, 360, updateMethodIndex=0)
    assert new < 0.2
    assert old > 2 * new


@pytest.mark.parametrize('index', range(len(METHODS)))
def test_default_step_converges_for_every_method(index: int):
    # The old default was also far too small for Cimmino and component
    # averaging: after 10 iterations their volumes were nearly empty
    # (RMSE about that of zeros, 0.63).
    assert reconstruct(32, 60, updateMethodIndex=index) < 0.25


def test_a_given_step_is_used_as_given():
    # A tiny step barely moves away from zero; the automatic one would
    assert reconstruct(32, 60, stepSize=1e-7, updateMethodIndex=0) > 0.6
