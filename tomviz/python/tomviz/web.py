import os
import shutil
import zipfile

from paraview import simple
from paraview.web.dataset_builder import ImageDataSetBuilder
from paraview.web.dataset_builder import CompositeDataSetBuilder


def web_export(destinationPath, exportType, deltaPhi, deltaTheta):
    dest = '%s/data' % destinationPath
    camera = {
        'type': 'spherical',
        'phi': range(0, 360, deltaPhi),
        'theta': range(-deltaTheta, deltaTheta + 1, deltaTheta)
    }

    # Choose export mode:
    if exportType == 0:
        export_images(dest, camera)

    if exportType == 1:
        export_layers(dest, camera)

    # Zip data directory
    zipData(destinationPath)

    # Copy application
    copy_viewer(destinationPath)

# -----------------------------------------------------------------------------
# Helpers
# -----------------------------------------------------------------------------


def zipData(destinationPath):
    dstFile = os.path.join(destinationPath, 'data.tomviz')
    dataDir = os.path.join(destinationPath, 'data')
    with zipfile.ZipFile(dstFile, mode='w') as zf
        for dirName, subdirList, fileList in os.walk(dataDir):
            for fname in fileList:
                fullPath = os.path.join(dirName, fname)
                relPath = 'data/%s' % (os.path.relpath(fullPath, dataDir))
                zf.write(fullPath, arcname=relPath,
                         compress_type=zipfile.ZIP_STORED)


    shutil.rmtree(dataDir)


def get_proxy(id):
    session = simple.servermanager.ActiveConnection.Session
    remoteObj = session.GetRemoteObject(int(id))
    return simple.servermanager._getPyProxy(remoteObj)


def copy_viewer(destinationPath):
    searchPath = os.getcwd()
    for root, dirs, files in os.walk(searchPath):
        if 'tomviz.html' in files and root != destinationPath:
            srcFile = os.path.join(root, 'tomviz.html')
            shutil.copy(srcFile, destinationPath)
            return


def add_scene_item(scene, name, proxy, view):
    hasNormal = False
    hasColor = False
    colors = {}
    representation = {}
    rep = simple.GetRepresentation(proxy, view)

    # Skip hidden object or volume
    if not rep.Visibility or rep.Representation == 'Volume':
        return

    for prop in ['Representation']:
        representation[prop] = rep.GetProperty(prop).GetData()

    pdInfo = proxy.GetPointDataInformation()
    numberOfPointArrays = pdInfo.GetNumberOfArrays()
    for idx in range(numberOfPointArrays):
        array = pdInfo.GetArray(idx)
        rangeValues = array.GetRange(-1)
        if array.Name == 'Normals':
            hasNormal = True
        if array.Name not in ['vtkValidPointMask', 'Normals']:
            hasColor = True
            if rangeValues[0] == rangeValues[1]:
                colors[array.Name] = {'constant': rangeValues[0]}
            else:
                colors[array.Name] = {
                    'location': 'POINT_DATA',
                    'range': [i for i in rangeValues]
                }

    # Get information about cell data arrays
    cdInfo = proxy.GetCellDataInformation()
    numberOfCellArrays = cdInfo.GetNumberOfArrays()
    for idx in range(numberOfCellArrays):
        array = cdInfo.GetArray(idx)
        hasColor = True
        colors[array.Name] = {
            'location': 'CELL_DATA',
            'range': array.GetRange(-1)
        }

    # Make sure Normals are available if lighting by normals
    source = proxy
    if not hasColor or rep.Representation == 'Outline':
        colors = {'solid': {'constant': 0}}
    elif 'normal' in scene['light'] and not hasNormal:
        rep.Visibility = 0
        surface = simple.ExtractSurface(Input=proxy)
        surfaceWithNormals = simple.GenerateSurfaceNormals(Input=surface)
        source = surfaceWithNormals

    scene['scene'].append({
        'name': name,
        'source': source,
        'colors': colors,
        'representation': representation
    })

# -----------------------------------------------------------------------------
# Image based exporter
# -----------------------------------------------------------------------------


def export_images(destinationPath, camera):
    view = simple.GetRenderView()
    idb = ImageDataSetBuilder(destinationPath, 'image/jpg', camera)
    idb.start(view)
    idb.writeImages()
    idb.stop()

# -----------------------------------------------------------------------------
# Composite exporter
# -----------------------------------------------------------------------------


def export_layers(destinationPath, camera):
    view = simple.GetRenderView()
    fp = tuple(view.CameraFocalPoint)
    cp = tuple(view.CameraPosition)
    vu = tuple(view.CameraViewUp)
    sceneDescription = {
        'size': tuple(view.ViewSize),
        'light': ['intensity'], # 'normal', intensity
        'camera': {
            'CameraViewUp': vu,
            'CameraPosition': cp,
            'CameraFocalPoint': fp
        },
        'scene': []
    }

    for key, value in simple.GetSources().iteritems():
        add_scene_item(sceneDescription, key[0], value, view)

    # Generate export
    dsb = CompositeDataSetBuilder(
        destinationPath, sceneDescription, camera, {}, {}, view)
    dsb.start()
    dsb.writeData()
    dsb.stop(compress=False)
