# -*- coding: utf-8 -*-

###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
"""Conversion shim between the app's VTK port payloads and the
tomviz_pipeline numpy payload classes.

The C++ pipeline keeps carrying vtkImageData/vtkTable/vtkMolecule
between nodes; Python scripts see only the library's numpy-backed
Dataset / Table / Molecule. This module is the single place the two
worlds meet:

- ``wrap_vtk_image``   — vtkImageData -> Dataset/LegacyDataset whose
  arrays are zero-copy numpy views over the VTK buffers. The image is
  stamped on the dataset so in-place mutations need no write-back and
  the VTK object outlives every view.
- ``flush_dataset``    — after a script ran, push replaced arrays and
  metadata back into the backing (or a given) vtkImageData.
- ``refresh_dataset``  — rebuild the numpy views after C++/VTK code
  mutated the backing image (the reverse of flush).
- ``payload_to_vtk``   — Dataset/Table/Molecule -> the corresponding
  VTK object for output ports and live-preview (progress.data)
  payloads; VTK objects pass through untouched.
- ``vtk_table_to_table`` / ``vtk_molecule_to_molecule`` — the input
  direction for Table/Molecule ports.
"""

import weakref

import numpy as np

# Importing the data-model module registers the wrapped Python types
# (vtkImageData, vtkTable, vtkMolecule, ...) with vtkPythonUtil — the
# C++ side relies on that when it casts VTK pointers into the Python
# objects handed to this module. Without it the cast degrades to a
# bare vtkObject wrapper.
import vtkmodules.vtkCommonDataModel  # noqa: F401
from vtkmodules.util import numpy_support as np_s
from vtkmodules.util.vtkConstants import VTK_DOUBLE, VTK_INT

from tomviz_pipeline.dataset import Dataset, LegacyDataset
from tomviz_pipeline.molecule import Molecule
from tomviz_pipeline.table import Table

# Backing vtkImageData for each wrapped dataset, keyed weakly by the
# dataset so it's released when the dataset dies. This is kept OUT of
# the dataset's instance dict on purpose: apply_to_each_array (and any
# other consumer) does copy.deepcopy() on datasets, and a VTK object
# stamped as an attribute would be deep-copied — invoking VTK methods
# and creating/destroying VTK objects on the pipeline worker thread
# during Python GC, which corrupts the heap. A deep-copied dataset is a
# fresh, independent object with NO backing (it materializes a new
# image on flush if ever needed), which is exactly what we want.
_backings: 'weakref.WeakKeyDictionary' = weakref.WeakKeyDictionary()


def _get_backing(dataset):
    return _backings.get(dataset)


def _set_backing(dataset, image):
    _backings[dataset] = image


def _as_views(image):
    """name -> numpy view over each single-component point-data array,
    shaped like the image (Fortran order). Multi-component arrays get a
    trailing component axis."""
    dims = tuple(image.GetDimensions())
    pd = image.GetPointData()
    arrays = {}
    for i in range(pd.GetNumberOfArrays()):
        vtkarray = pd.GetAbstractArray(i)
        if vtkarray is None or not hasattr(vtkarray, 'GetNumberOfTuples'):
            continue
        name = vtkarray.GetName() or f'Scalars_{i}'
        flat = np_s.vtk_to_numpy(vtkarray)
        if flat.ndim == 1:
            arrays[name] = flat.reshape(dims, order='F')
        else:
            arrays[name] = flat.reshape(
                dims + (flat.shape[1],), order='F')
    return arrays


def _field_array(image, name):
    fd = image.GetFieldData()
    vtkarray = fd.GetArray(name)
    if vtkarray is None:
        return None
    return np_s.vtk_to_numpy(vtkarray)


def _set_field_array(image, name, values, vtk_type):
    fd = image.GetFieldData()
    fd.RemoveArray(name)
    if values is None:
        return
    vtkarray = np_s.numpy_to_vtk(np.asarray(values).ravel(), deep=1,
                                 array_type=vtk_type)
    vtkarray.SetName(name)
    fd.AddArray(vtkarray)


def wrap_vtk_image(image, legacy=False):
    """Wrap a vtkImageData in a library Dataset (or LegacyDataset for
    v1 operator scripts). Arrays are numpy views over the VTK buffers;
    tilt angles / scan ids come from the field-data convention."""
    cls = LegacyDataset if legacy else Dataset

    active = None
    scalars = image.GetPointData().GetScalars()
    if scalars is not None and scalars.GetName():
        active = scalars.GetName()

    dataset = cls(_as_views(image), active)
    dataset.spacing = tuple(image.GetSpacing())

    tilt_angles = _field_array(image, 'tilt_angles')
    if tilt_angles is not None:
        dataset.tilt_angles = np.array(tilt_angles, dtype=np.float64)
        dataset.tilt_axis = 2
    scan_ids = _field_array(image, 'scan_ids')
    if scan_ids is not None:
        dataset.scan_ids = np.array(scan_ids)

    _set_backing(dataset, image)
    return dataset


def _same_buffer(a, b) -> bool:
    """True when two numpy arrays alias the same memory block."""
    return (a.__array_interface__['data'][0] ==
            b.__array_interface__['data'][0] and a.nbytes == b.nbytes)


def flush_dataset(dataset, image=None):
    """Push a dataset's arrays and metadata into ``image`` (default:
    its backing image). Arrays still aliasing the VTK buffers are
    skipped — in-place mutations are already there; replaced arrays are
    deep-copied into VTK-owned memory (what the internal_dataset
    setters used to do)."""
    if image is None:
        image = _get_backing(dataset)
    if image is None:
        raise ValueError('flush_dataset: no backing vtkImageData')

    arrays = dataset.arrays
    if not arrays:
        return image

    first = next(iter(arrays.values()))
    dims = tuple(first.shape[:3])
    dims_changed = dims != tuple(image.GetDimensions())
    if dims_changed:
        image.SetDimensions(*dims)

    pd = image.GetPointData()

    # Drop stale arrays (removed by the script, or size-invalidated by
    # a dimension change).
    for i in reversed(range(pd.GetNumberOfArrays())):
        existing = pd.GetAbstractArray(i)
        name = existing.GetName() if existing is not None else None
        if name not in arrays or dims_changed:
            pd.RemoveArray(i)

    for name, arr in arrays.items():
        existing = pd.GetArray(name)
        if existing is not None and not dims_changed:
            view = np_s.vtk_to_numpy(existing)
            if _same_buffer(view, np.asarray(arr)):
                continue
        replacement = np.asarray(arr)
        if replacement.ndim == 4:
            # A multi-component array (dims + components, as _as_views
            # shapes it): VTK stores it tuple by tuple with x fastest,
            # so the components have to interleave, not trail.
            comps = replacement.shape[3]
            flat = replacement.transpose(3, 0, 1, 2).ravel(order='F')
            tuples = np.ascontiguousarray(flat.reshape(-1, comps))
            vtkarray = np_s.numpy_to_vtk(tuples, deep=1)
        else:
            contiguous = np.asfortranarray(replacement)
            vtkarray = np_s.numpy_to_vtk(contiguous.ravel(order='A'),
                                         deep=1)
        vtkarray.SetName(name)
        pd.RemoveArray(name)
        pd.AddArray(vtkarray)

    if dataset.active_name in arrays:
        pd.SetActiveScalars(dataset.active_name)
    else:
        # e.g. a child dataset that inherited the parent's active name
        # but was populated under different array names.
        pd.SetActiveScalars(next(iter(arrays)))

    if dataset.spacing is not None:
        image.SetSpacing(*tuple(dataset.spacing))

    _set_field_array(image, 'tilt_angles', dataset.tilt_angles,
                     VTK_DOUBLE)
    _set_field_array(image, 'scan_ids', dataset.scan_ids, VTK_INT)

    image.Modified()
    return image


def refresh_dataset(dataset):
    """Rebuild the dataset's numpy views from its backing image, after
    C++/VTK code mutated the image in place (reverse of flush)."""
    image = _get_backing(dataset)
    if image is None:
        raise ValueError('refresh_dataset: no backing vtkImageData')
    dataset.arrays.clear()
    dataset.arrays.update(_as_views(image))
    scalars = image.GetPointData().GetScalars()
    if scalars is not None and scalars.GetName():
        dataset.active_name = scalars.GetName()
    dataset.spacing = tuple(image.GetSpacing())
    tilt_angles = _field_array(image, 'tilt_angles')
    dataset.tilt_angles = (np.array(tilt_angles, dtype=np.float64)
                           if tilt_angles is not None else None)
    return dataset


def _table_to_vtk(table):
    from vtk import vtkStringArray, vtkTable, vtkUnsignedCharArray
    vtk_table = vtkTable()
    for name, column in table.columns.items():
        if isinstance(column, list):
            array = vtkStringArray()
            array.SetName(name)
            array.SetNumberOfValues(len(column))
            for i, v in enumerate(column):
                array.SetValue(i, str(v))
        else:
            array = np_s.numpy_to_vtk(np.asarray(column).ravel(), deep=1)
            array.SetName(name)
        vtk_table.AddColumn(array)

    # Chart metadata travels as field-data arrays, the convention the
    # in-app chart viewers read (see the old make_spreadsheet).
    if table.axes_labels is not None:
        label_array = vtkStringArray()
        label_array.SetName('axes_labels')
        label_array.SetNumberOfValues(2)
        label_array.SetValue(0, str(table.axes_labels[0]))
        label_array.SetValue(1, str(table.axes_labels[1]))
        vtk_table.GetFieldData().AddArray(label_array)
    if table.axes_log_scale is not None:
        log_array = vtkUnsignedCharArray()
        log_array.SetName('axes_log_scale')
        log_array.SetNumberOfComponents(1)
        log_array.SetNumberOfTuples(2)
        log_array.SetValue(0, int(table.axes_log_scale[0]))
        log_array.SetValue(1, int(table.axes_log_scale[1]))
        vtk_table.GetFieldData().AddArray(log_array)
    return vtk_table


def _molecule_to_vtk(molecule):
    from vtk import vtkMolecule
    vtk_molecule = vtkMolecule()
    for number, pos in zip(molecule.atomic_numbers, molecule.positions):
        vtk_molecule.AppendAtom(int(number), float(pos[0]),
                                float(pos[1]), float(pos[2]))
    for (begin, end), order in zip(molecule.bonds, molecule.bond_orders):
        vtk_molecule.AppendBond(int(begin), int(end), int(order))
    return vtk_molecule


def _dataset_to_vtk(dataset):
    image = _get_backing(dataset)
    if image is None:
        from vtk import vtkImageData
        image = vtkImageData()
        if dataset.arrays:
            first = next(iter(dataset.arrays.values()))
            image.SetDimensions(*first.shape[:3])
        # Adopt the new image as the dataset's backing so later
        # flush/refresh calls (and repeated conversions) reuse it.
        _set_backing(dataset, image)
    return flush_dataset(dataset, image)


def payload_to_vtk(payload):
    """Convert a library payload (Dataset / Table / Molecule) to the
    corresponding VTK object; VTK objects (duck-typed via ``IsA``) pass
    through untouched. Returns None for unrecognized payloads."""
    if payload is None:
        return None
    if hasattr(payload, 'IsA'):
        # Already a VTK object (scripts building vtk directly).
        return payload
    if isinstance(payload, Dataset):
        return _dataset_to_vtk(payload)
    if isinstance(payload, Table):
        return _table_to_vtk(payload)
    if isinstance(payload, Molecule):
        return _molecule_to_vtk(payload)
    return None


def vtk_table_to_table(vtk_table):
    """vtkTable -> library Table (copies; tables are small)."""
    table = Table()
    for i in range(vtk_table.GetNumberOfColumns()):
        column = vtk_table.GetColumn(i)
        if column is None:
            continue
        name = column.GetName() or f'column_{i}'
        if hasattr(column, 'GetValue') and not hasattr(column,
                                                       'GetTuple1'):
            values = [column.GetValue(j)
                      for j in range(column.GetNumberOfValues())]
            table.set_column(name, values)
        else:
            table.set_column(name, np.array(np_s.vtk_to_numpy(column)))

    fd = vtk_table.GetFieldData()
    labels = fd.GetAbstractArray('axes_labels')
    if labels is not None and labels.GetNumberOfValues() >= 2:
        table.axes_labels = (labels.GetValue(0), labels.GetValue(1))
    log_scale = fd.GetArray('axes_log_scale')
    if log_scale is not None and log_scale.GetNumberOfTuples() >= 2:
        table.axes_log_scale = (bool(log_scale.GetValue(0)),
                                bool(log_scale.GetValue(1)))
    return table


def vtk_molecule_to_molecule(vtk_molecule):
    """vtkMolecule -> library Molecule (copies; molecules are small)."""
    num_atoms = vtk_molecule.GetNumberOfAtoms()
    num_bonds = vtk_molecule.GetNumberOfBonds()
    atomic_numbers = np.empty(num_atoms, dtype=np.uint16)
    positions = np.empty((num_atoms, 3), dtype=np.float32)
    for i in range(num_atoms):
        atom = vtk_molecule.GetAtom(i)
        atomic_numbers[i] = atom.GetAtomicNumber()
        positions[i] = atom.GetPosition()
    bonds = np.empty((num_bonds, 2), dtype=np.int64)
    bond_orders = np.empty(num_bonds, dtype=np.uint16)
    for i in range(num_bonds):
        bond = vtk_molecule.GetBond(i)
        bonds[i, 0] = bond.GetBeginAtomId()
        bonds[i, 1] = bond.GetEndAtomId()
        bond_orders[i] = vtk_molecule.GetBondOrder(i)
    return Molecule(atomic_numbers, positions, bonds, bond_orders)
