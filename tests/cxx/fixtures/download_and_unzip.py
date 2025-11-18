import os
from pathlib import Path
import sys
import urllib.request
import zipfile

if len(sys.argv) < 3:
    sys.exit('Usage: <script> <url> <extract_dir>')

url = sys.argv[1]
extract_dir = Path(sys.argv[2])

filename = Path('downloaded_file.zip')
if filename.exists():
    filename.unlink()

last_progress = -1

def download_progress_hook(block_num, block_size, total_size):
    global last_progress
    downloaded = block_num * block_size
    downloaded_mb = downloaded / 1024 / 1024
    total_size_mb = total_size / 1024 / 1024
    progress = int((downloaded / total_size) * 100) if total_size > 0 else 0
    if progress > last_progress:
        last_progress = progress
        print(f"\rDownload Progress: {progress}% ({downloaded_mb:.2f}/{total_size_mb:.2f} MB)")

print(f'Downloading "{url}" to "{filename}"')
urllib.request.urlretrieve(url, filename, reporthook=download_progress_hook)
print('\n')

extract_dir.mkdir(parents=True, exist_ok=True)

print(f'Unzipping "{filename}" to "{extract_dir}"')
with zipfile.ZipFile(filename, 'r') as zip_ref:
    zip_ref.extractall(extract_dir)
