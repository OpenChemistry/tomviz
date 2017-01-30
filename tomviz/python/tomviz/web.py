from paraview import simple
from paraview.web.dataset_builder import *

def copy_viewer(destinationPath):
    # FIXME: Copy Web viewer
    pass

def web_export(destinationPath):
    dest = '%s/data' % destinationPath
    camera =  {'type': 'spherical', 'phi': range(0, 360, 30), 'theta': range(-60, 61, 10)}
    export_images(dest, camera)
    copy_viewer(destinationPath)
    

def export_images(destinationPath, camera):
    idb = ImageDataSetBuilder(destinationPath, 'image/jpg', camera)
    view = simple.GetRenderView()
    idb.start(view)
    idb.writeImages()
    idb.stop()