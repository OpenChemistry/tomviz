import time
from PIL import Image
from . import test_image

img = Image.open(test_image())

connected = False


def connect():
    global connected
    connected = True
    return connected


def disconnect():
    global connected
    connected = False
    return connected


def set_tilt_angle(angle):
    time.sleep(2)
    current_frame = max(0, min(angle, img.n_frames-1))
    img.seek(current_frame)
    return angle


def set_acquisition_params(**params):
    current_params = {
        'test': 1,
        'foo': 'foo'
    }
    current_params.update(params)

    return current_params


def preview_scan():
    time.sleep(2)
    return img.tobytes()


def stem_acquire():
    time.sleep(3)
    return img.tobytes()
