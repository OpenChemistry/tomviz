import time
import requests
from PIL import Image

# Get the test data
response = requests.get(
    'https://data.kitware.com/api/v1/file/5893921d8d777f07219fca7e/download',
    stream=True)
img = Image.open(response.raw)


def set_tilt_angle(angle):
    time.sleep(2)
    current_frame = max(0, min(angle, img.n_frames-1))
    img.seek(current_frame)
    return angle


def preview_scan():
    time.sleep(2)
    return img.tobytes()


def stem_acquire():
    time.sleep(5)
    return img.tobytes()
