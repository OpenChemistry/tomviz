import numpy as np
import pytest

from tomviz_pipeline import PortData
from tomviz_pipeline.dataset import Dataset
from tomviz_pipeline.nodes.transforms.legacy_python import (
    LegacyPythonTransform,
)

from utils import OPERATOR_PATH


def _run(name, arr, port_type='LabelMap', spacing=(1.0, 1.0, 1.0),
         **arguments):
    t = LegacyPythonTransform()
    t.deserialize({
        'description': (OPERATOR_PATH / f'{name}.json').read_text(),
        'script': (OPERATOR_PATH / f'{name}.py').read_text(),
        'arguments': arguments,
    })
    ds = Dataset({'ImageScalars': np.asfortranarray(arr)}, 'ImageScalars')
    ds.spacing = spacing
    result = t.transform({'volume': PortData(ds, port_type)})
    return result[t.output_ports()[0].name].payload.active_scalars


def _cube(size=12, lo=4, hi=8):
    arr = np.zeros((size, size, size), dtype=np.uint8)
    arr[lo:hi, lo:hi, lo:hi] = 1
    return arr


# --- Dilate / Erode ---

def test_box_dilation_grows_by_the_radius():
    out = _run('BinaryDilate', _cube(), structuring_element_id=0, radius=1)
    assert out.dtype == np.uint8
    assert out.sum() == 6 ** 3
    assert np.all(out[3:9, 3:9, 3:9] == 1)


def test_ball_dilation_is_smaller_than_box_dilation():
    box = _run('BinaryDilate', _cube(), structuring_element_id=0, radius=2)
    ball = _run('BinaryDilate', _cube(), structuring_element_id=1, radius=2)
    cross = _run('BinaryDilate', _cube(), structuring_element_id=2, radius=2)
    assert cross.sum() < ball.sum() < box.sum()
    # The ball rounds the corners off
    assert box[2, 2, 2] == 1 and ball[2, 2, 2] == 0


def test_erosion_shrinks_and_removes_thin_parts():
    arr = _cube()
    arr[2, 6, 6] = 1  # an isolated voxel
    out = _run('BinaryErode', arr, radius=1)
    assert out.sum() == 2 ** 3
    assert np.all(out[5:7, 5:7, 5:7] == 1)
    assert out[2, 6, 6] == 0


def test_objects_touching_the_edge_keep_their_edge():
    arr = np.zeros((8, 8, 8), dtype=np.uint8)
    arr[0:4, 2:6, 2:6] = 1
    out = _run('BinaryErode', arr, radius=1)
    # Eroded from the free faces only, as ITK does by default
    assert np.all(out[0:3, 3:5, 3:5] == 1)
    assert out.sum() == 3 * 2 * 2


def test_other_labels_are_overwritten_only_where_reached():
    arr = _cube()
    arr[0, 0, 0] = 7
    arr[8, 8, 8] = 7      # touches the object, so the dilation takes it
    out = _run('BinaryDilate', arr, radius=1, object_label=1,
               background_label=0)
    assert out[0, 0, 0] == 7 and out[8, 8, 8] == 1
    out = _run('BinaryErode', arr, radius=1, object_label=7,
               background_label=0)
    # Label 7 voxels are isolated, so they erode away; label 1 untouched
    assert out[0, 0, 0] == 0 and out[8, 8, 8] == 0
    assert np.array_equal(out[4:8, 4:8, 4:8], np.ones((4, 4, 4)))


# --- Open / Close ---

def test_opening_removes_specks_and_keeps_the_body():
    arr = _cube()
    arr[1, 1, 1] = 1
    out = _run('BinaryOpen', arr, radius=1)
    assert out[1, 1, 1] == 0
    assert np.array_equal(out, _cube())


def test_closing_fills_a_hole():
    arr = _cube(size=14, lo=2, hi=12)
    arr[7, 7, 7] = 0
    out = _run('BinaryClose', arr, radius=1)
    assert out[7, 7, 7] == 1
    assert np.array_equal(out, _cube(size=14, lo=2, hi=12))


# --- ITK equivalence ---

@pytest.mark.parametrize('shape', [0, 1, 2])
def test_dilate_and_erode_match_itk_when_available(shape):
    itk = pytest.importorskip('itk')
    rng = np.random.default_rng(3)
    arr = (rng.random((14, 13, 12)) < 0.3).astype(np.uint8)
    arr[rng.random(arr.shape) < 0.05] = 2  # a third label, left alone

    kernel_type = itk.FlatStructuringElement[3]
    kernel = [kernel_type.Box, kernel_type.Ball, kernel_type.Cross][shape](2)

    def reference(filter_class, value_setter):
        image = itk.GetImageViewFromArray(np.ascontiguousarray(arr))
        f = filter_class[type(image), type(image), kernel_type].New()
        value_setter(f)
        f.SetBackgroundValue(0)
        f.SetKernel(kernel)
        f.SetInput(image)
        f.Update()
        return itk.GetArrayFromImage(f.GetOutput())

    dilated = _run('BinaryDilate', arr, structuring_element_id=shape,
                   radius=2)
    assert np.array_equal(
        dilated, reference(itk.BinaryDilateImageFilter,
                           lambda f: f.SetDilateValue(1)))
    eroded = _run('BinaryErode', arr, structuring_element_id=shape, radius=2)
    assert np.array_equal(
        eroded, reference(itk.BinaryErodeImageFilter,
                          lambda f: f.SetErodeValue(1)))

    # Opening and closing are the two composed, in either order
    def compose(first, first_value, second, second_value, data):
        image = itk.GetImageViewFromArray(np.ascontiguousarray(data))
        a = first[type(image), type(image), kernel_type].New()
        first_value(a)
        a.SetBackgroundValue(0)
        a.SetKernel(kernel)
        a.SetInput(image)
        b = second[type(image), type(image), kernel_type].New()
        second_value(b)
        b.SetBackgroundValue(0)
        b.SetKernel(kernel)
        b.SetInput(a.GetOutput())
        b.Update()
        return itk.GetArrayFromImage(b.GetOutput())

    opened = _run('BinaryOpen', arr, structuring_element_id=shape, radius=2)
    assert np.array_equal(
        opened, compose(itk.BinaryErodeImageFilter,
                        lambda f: f.SetErodeValue(1),
                        itk.BinaryDilateImageFilter,
                        lambda f: f.SetDilateValue(1), arr))
    closed = _run('BinaryClose', arr, structuring_element_id=shape, radius=2)
    assert np.array_equal(
        closed, compose(itk.BinaryDilateImageFilter,
                        lambda f: f.SetDilateValue(1),
                        itk.BinaryErodeImageFilter,
                        lambda f: f.SetErodeValue(1), arr))


# --- Unsharp Mask ---

def _blurry_step():
    x = np.linspace(-3, 3, 24)
    step = 1.0 / (1.0 + np.exp(-2 * x))
    return np.broadcast_to(step, (8, 8, 24)).astype(np.float32).copy()


def test_unsharp_mask_sharpens_an_edge_and_amount_zero_is_identity():
    arr = _blurry_step()
    same = _run('UnsharpMask', arr, 'ImageData', amount=0.0, sigma=1.0)
    assert np.allclose(same, arr, atol=1e-6)
    sharp = _run('UnsharpMask', arr, 'ImageData', amount=1.0, sigma=1.0)
    assert sharp.dtype == np.float32
    # Steeper at the edge, with over- and undershoot either side
    assert sharp[4, 4, 13] - sharp[4, 4, 10] > arr[4, 4, 13] - arr[4, 4, 10]
    assert sharp.max() > arr.max() and sharp.min() < arr.min()


def test_unsharp_mask_threshold_leaves_small_differences_alone():
    arr = _blurry_step()
    out = _run('UnsharpMask', arr, 'ImageData', amount=1.0, sigma=1.0,
               threshold=1000.0)
    assert np.allclose(out, arr, atol=1e-6)


def test_unsharp_mask_clamps_integer_data():
    arr = (_blurry_step() * 255).astype(np.uint8)
    out = _run('UnsharpMask', arr, 'ImageData', amount=2.0, sigma=1.5)
    assert out.dtype == np.uint8
    assert out.max() == 255 and out.min() == 0


def test_unsharp_mask_sigma_is_in_physical_units():
    arr = _blurry_step()
    fine = _run('UnsharpMask', arr, 'ImageData', amount=1.0, sigma=1.0,
                spacing=(1.0, 1.0, 1.0))
    coarse = _run('UnsharpMask', arr, 'ImageData', amount=1.0, sigma=1.0,
                  spacing=(2.0, 2.0, 2.0))
    # Twice the spacing halves the blur in voxels, so less sharpening
    assert np.abs(coarse - arr).max() < np.abs(fine - arr).max()


def test_unsharp_mask_agrees_with_itk_when_available():
    itk = pytest.importorskip('itk')
    arr = _blurry_step()
    out = _run('UnsharpMask', arr, 'ImageData', amount=0.8, sigma=2.0)

    image = itk.GetImageViewFromArray(np.ascontiguousarray(arr))
    f = itk.UnsharpMaskImageFilter.New(Input=image)
    f.SetAmount(0.8)
    f.SetThreshold(0.0)
    f.SetSigma(2.0)
    f.Update()
    reference = itk.GetArrayFromImage(f.GetOutput())
    # ITK blurs with a recursive approximation of the Gaussian; the two
    # agree to within a few percent of the sharpening they add
    assert np.abs(out - reference).max() < 0.05 * np.abs(out - arr).max()
