###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
"""Tests for tomviz._boundary — the VTK <-> tomviz_pipeline numpy
conversion shim used by the in-app Python script boundary."""

import copy

import numpy as np
import pytest

vtk = pytest.importorskip('vtk')

from vtkmodules.util import numpy_support as np_s  # noqa: E402

from tomviz import _boundary  # noqa: E402
from tomviz_pipeline.dataset import Dataset, LegacyDataset  # noqa: E402
from tomviz_pipeline.table import Table  # noqa: E402
from tomviz_pipeline.molecule import Molecule  # noqa: E402
from tomviz.utils import apply_to_each_array  # noqa: E402


def _make_image(dims=(3, 4, 5), arrays=(('Scalars', 2.0),)):
    img = vtk.vtkImageData()
    img.SetDimensions(*dims)
    img.SetSpacing(1.0, 2.0, 3.0)
    for name, value in arrays:
        flat = np.full(dims, value, dtype=np.float32).ravel(order='F')
        va = np_s.numpy_to_vtk(flat, deep=1)
        va.SetName(name)
        img.GetPointData().AddArray(va)
    img.GetPointData().SetActiveScalars(arrays[0][0])
    return img


def test_wrap_exposes_views_and_metadata():
    img = _make_image()
    ds = _boundary.wrap_vtk_image(img, legacy=True)
    assert isinstance(ds, LegacyDataset)
    assert ds.active_scalars.shape == (3, 4, 5)
    assert ds.spacing == (1.0, 2.0, 3.0)
    # In-place mutation of the view lands in the VTK buffer without a
    # flush (it aliases the same memory).
    ds.active_scalars[0, 0, 0] = 42.0
    back = np_s.vtk_to_numpy(img.GetPointData().GetScalars())
    assert back[0] == 42.0


def test_flush_replaced_array_and_metadata():
    img = _make_image()
    ds = _boundary.wrap_vtk_image(img)
    ds.active_scalars = np.zeros((2, 2, 2), dtype=np.float32) + 7.0
    ds.tilt_angles = np.array([0.0, 1.0])
    _boundary.flush_dataset(ds)
    assert tuple(img.GetDimensions()) == (2, 2, 2)
    assert np_s.vtk_to_numpy(img.GetPointData().GetScalars())[0] == 7.0
    ta = np_s.vtk_to_numpy(img.GetFieldData().GetArray('tilt_angles'))
    assert list(ta) == [0.0, 1.0]


def test_flush_replaced_multi_component_array_keeps_its_components():
    # An RGB stack is one array of three components per voxel. Replacing
    # it has to come back as such, with the components interleaved the
    # way VTK stores them, not as one long single-component array.
    dims = (3, 4, 5)
    comps = 3
    n = dims[0] * dims[1] * dims[2]
    img = vtk.vtkImageData()
    img.SetDimensions(*dims)
    tuples = np.arange(n * comps, dtype=np.float32).reshape(n, comps)
    va = np_s.numpy_to_vtk(tuples, deep=1)
    va.SetName('RGB')
    img.GetPointData().SetScalars(va)

    ds = _boundary.wrap_vtk_image(img)
    view = ds.active_scalars
    assert view.shape == dims + (comps,)
    x, y, z = 1, 2, 3
    t = x + dims[0] * (y + dims[1] * z)
    assert view[x, y, z, 1] == tuples[t, 1]

    ds.active_scalars = view * 2
    _boundary.flush_dataset(ds)
    out = img.GetPointData().GetScalars()
    assert out.GetNumberOfComponents() == comps
    assert out.GetNumberOfTuples() == n
    flushed = np_s.vtk_to_numpy(out)
    assert np.array_equal(flushed, tuples * 2)
    # And the views over the new buffer read the same way again
    assert _boundary.wrap_vtk_image(img).active_scalars[x, y, z, 1] == \
        2 * tuples[t, 1]


def test_deepcopy_does_not_clone_backing_vtk():
    """Regression: a deep-copied wrapped dataset must NOT carry the VTK
    backing image. Otherwise copy.deepcopy (used by
    apply_to_each_array) clones vtkImageData on the pipeline worker
    thread, creating/destroying VTK objects during Python GC and
    corrupting the heap (in-app SIGSEGV on the first multi-array
    transform)."""
    img = _make_image(arrays=(('Pt_L', 1.0), ('Zn_K', 2.0)))
    ds = _boundary.wrap_vtk_image(img, legacy=True)
    assert _boundary._get_backing(ds) is not None

    ds2 = copy.deepcopy(ds)
    # No backing carried over — the copy is independent.
    assert _boundary._get_backing(ds2) is None
    # And its arrays are owned deep copies, not views over VTK memory.
    assert ds2.arrays['Pt_L'].flags.owndata


def test_apply_to_each_array_over_wrapped_multi_array_dataset():
    """The exact in-app crash scenario, at the Python level: a
    multi-array wrapped dataset run through an @apply_to_each_array
    operator. Must complete and mutate every array, without cloning the
    backing image during the per-array deepcopies."""
    img = _make_image(arrays=(('Pt_L', 1.0), ('Zn_K', 2.0), ('Fe_K', 3.0)))
    ds = _boundary.wrap_vtk_image(img, legacy=True)

    @apply_to_each_array
    def add_ten(dataset):
        dataset.active_scalars = dataset.active_scalars + 10.0

    add_ten(ds)
    _boundary.flush_dataset(ds)

    pd = img.GetPointData()
    assert pd.GetNumberOfArrays() == 3
    for name, base in (('Pt_L', 1.0), ('Zn_K', 2.0), ('Fe_K', 3.0)):
        arr = np_s.vtk_to_numpy(pd.GetArray(name))
        np.testing.assert_allclose(arr, base + 10.0)


def test_payload_to_vtk_dataset_table_molecule():
    ds = Dataset({'A': np.arange(8, dtype=np.float32).reshape(2, 2, 2)}, 'A')
    ds.spacing = (2.0, 2.0, 2.0)
    img = _boundary.payload_to_vtk(ds)
    assert img.IsA('vtkImageData')
    assert tuple(img.GetDimensions()) == (2, 2, 2)

    table = Table()
    table.set_column('x', np.array([1.0, 2.0], dtype=np.float32))
    vt = _boundary.payload_to_vtk(table)
    assert vt.IsA('vtkTable')
    assert vt.GetNumberOfColumns() == 1

    mol = Molecule([6, 1], [[0, 0, 0], [1, 0, 0]], bonds=[[0, 1]])
    vm = _boundary.payload_to_vtk(mol)
    assert vm.IsA('vtkMolecule')
    assert vm.GetNumberOfAtoms() == 2

    # VTK objects pass through untouched.
    assert _boundary.payload_to_vtk(vm) is vm


def test_vtk_table_and_molecule_to_library():
    vt = vtk.vtkTable()
    col = vtk.vtkFloatArray()
    col.SetName('radius')
    col.SetNumberOfValues(2)
    col.SetValue(0, 1.5)
    col.SetValue(1, 2.5)
    vt.AddColumn(col)
    table = _boundary.vtk_table_to_table(vt)
    assert isinstance(table, Table)
    np.testing.assert_allclose(table.column('radius'), [1.5, 2.5])

    vm = vtk.vtkMolecule()
    vm.AppendAtom(6, 0.0, 0.0, 0.0)
    vm.AppendAtom(1, 1.0, 0.0, 0.0)
    vm.AppendBond(0, 1, 1)
    mol = _boundary.vtk_molecule_to_molecule(vm)
    assert isinstance(mol, Molecule)
    assert mol.num_atoms == 2
    assert mol.num_bonds == 1
