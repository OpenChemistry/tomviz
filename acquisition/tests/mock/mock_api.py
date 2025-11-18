import time
from PIL import Image
from . import test_image, test_black_image, angle_to_page
from tomviz_acquisition.acquisition.utility import tobytes

img = Image.open(test_image())
black = test_black_image().read()

connected = False
current_frame = None


def connect():
    global connected
    connected = True
    return connected


def disconnect():
    global connected
    connected = False
    return connected


def set_tilt_angle(angle):
    global current_frame
    time.sleep(2)
    (current_frame, set_angle) = angle_to_page(angle)
    return set_angle


def set_acquisition_params(**params):
    current_params = {
        'test': 1,
        'foo': 'foo'
    }
    current_params.update(params)

    return current_params


def preview_scan():
    data = black
    if current_frame:
        img.seek(current_frame)
        data = tobytes(img)
    time.sleep(2)
    return data


def stem_acquire():
    data = black
    if current_frame:
        img.seek(current_frame)
        data = tobytes(img)
    time.sleep(3)
    return data
