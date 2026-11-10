import numpy as np
import pytest

from utils import load_operator_class, load_operator_module

from tomviz_pipeline.dataset import LegacyDataset as Dataset


class CountUpdates:
    """Stands in for an operator's progress and counts the live updates."""

    def __init__(self):
        self.updates = 0
        self.maximum = 0
        self.value = 0
        self.message = ''

    @property
    def data(self):
        return None

    @data.setter
    def data(self, value):
        self.updates += 1


def tilt_series(Nslice):
    rng = np.random.default_rng(0)
    angles = np.arange(-60, 61, 10.0)
    ds = Dataset({'scalars': np.asfortranarray(
        rng.random((Nslice, 16, len(angles)), dtype=np.float32))})
    ds.spacing = [1.0, 1.0, 1.0]
    ds.tilt_angles = angles
    return ds


def run(name, Nslice, **kwargs):
    operator = load_operator_class(load_operator_module(name))
    operator.progress = CountUpdates()
    out = operator.transform(tilt_series(Nslice), **kwargs)
    assert out['reconstruction'].active_scalars.shape == (Nslice, 16, 16)
    return operator.progress.updates


# Each count includes the final update every operator sends at the end.
@pytest.mark.parametrize('Nupdates,updates', [
    (0, 1), (1, 2), (3, 4),
    # More updates than slices used to divide by zero: one per slice.
    (20, 4),
    # The default in transform(), for calls from scripts.
    (None, 1),
])
def test_wbp_live_updates(Nupdates, updates):
    assert run('Recon_WBP', 3, Nrecon=16, filter=1, interp=0,
               Nupdates=Nupdates) == updates


@pytest.mark.parametrize('name', ['Recon_ART', 'Recon_SIRT'])
@pytest.mark.parametrize('Nupdates,updates', [(0, 1), (50, 6), (100, 6)])
def test_art_and_sirt_live_updates_with_few_slices(name, Nupdates, updates):
    # 3 slices: the first iteration updated every Nslice // 4 slices,
    # which divided by zero with fewer than 4 slices. Now one update per
    # slice in the first iteration, one per iteration, and the final one.
    assert run(name, 3, Niter=2, Nupdates=Nupdates) == updates
