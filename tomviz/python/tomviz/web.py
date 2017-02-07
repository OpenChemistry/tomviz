import base64
import os
import shutil
import zipfile

from paraview import simple
from paraview.web.dataset_builder import ImageDataSetBuilder
from paraview.web.dataset_builder import CompositeDataSetBuilder

DATA_DIRECTORY = 'data'
DATA_FILENAME = 'data.tomviz'
BASE64_DATA_FILENAME = 'data.tomviz.base64'
HTML_FILENAME = 'tomviz.html'
HTML_WITH_DATA_FILENAME = 'tomviz_data.html'


def web_export(executionPath, destPath, exportType, nbPhi, nbTheta):
    # Destination directory for data
    dest = '%s/data' % destPath

    # Extract initial setting for view
    view = simple.GetRenderView()
    viewState = {}
    for prop in ['CameraViewUp', 'CameraPosition']:
        viewState[prop] = tuple(view.GetProperty(prop).GetData())

    # Camera handling
    deltaPhi = 360 / nbPhi
    deltaTheta = int(180 / nbTheta)
    thetaMax = deltaTheta
    while thetaMax + deltaTheta < 90:
        thetaMax += deltaTheta
    camera = {
        'type': 'spherical',
        'phi': range(0, 360, deltaPhi),
        'theta': range(-thetaMax, thetaMax + 1, deltaTheta)
    }

    # Choose export mode:
    if exportType == 0:
        export_images(dest, camera)

    if exportType == 1:
        export_volume_exploration_images(dest, camera)

    if exportType == 2:
        export_contour_exploration_images(dest, camera)

    if exportType == 3:
        export_layers(dest, camera)

    # Zip data directory
    zipData(destPath)

    # Copy application
    copy_viewer(destPath, executionPath)
    create_standalone_html(destPath)

    # Restore initial parameters
    for prop in viewState:
        view.GetProperty(prop).SetData(viewState[prop])

# -----------------------------------------------------------------------------
# Helpers
# -----------------------------------------------------------------------------


def zipData(destinationPath):
    dstFile = os.path.join(destinationPath, DATA_FILENAME)
    dataDir = os.path.join(destinationPath, DATA_DIRECTORY)

    if os.path.exists(dataDir):
        with zipfile.ZipFile(dstFile, mode='w') as zf:
            for dirName, subdirList, fileList in os.walk(dataDir):
                for fname in fileList:
                    fullPath = os.path.join(dirName, fname)
                    filePath = os.path.relpath(fullPath, dataDir)
                    relPath = '%s/%s' % (DATA_DIRECTORY, filePath)
                    zf.write(fullPath, arcname=relPath,
                             compress_type=zipfile.ZIP_STORED)

        shutil.rmtree(dataDir)


def get_proxy(id):
    session = simple.servermanager.ActiveConnection.Session
    remoteObj = session.GetRemoteObject(int(id))
    return simple.servermanager._getPyProxy(remoteObj)


def copy_viewer(destinationPath, executionPath):
    searchPath = executionPath
    for upDirTry in range(4):
        searchPath = os.path.normpath(os.path.join(searchPath, '..'))
        for root, dirs, files in os.walk(searchPath):
            if HTML_FILENAME in files and root != destinationPath:
                srcFile = os.path.join(root, HTML_FILENAME)
                shutil.copy(srcFile, destinationPath)
                return


def create_standalone_html(destinationPath):
    tmpData = os.path.join(destinationPath, BASE64_DATA_FILENAME)
    with file(tmpData, mode='w') as dataOut:
        with file(os.path.join(destinationPath, DATA_FILENAME)) as dataIn:
            base64.encode(dataIn, dataOut)

    srcHtmlPath = os.path.join(destinationPath, HTML_FILENAME)
    dstHtmlPath = os.path.join(destinationPath, HTML_WITH_DATA_FILENAME)
    with file(tmpData) as data:
        with file(srcHtmlPath) as srcHtml:
            with file(dstHtmlPath, mode='w') as dstHtml:
                for line in srcHtml:
                    if '<script type="text/javascript">' in line:
                        dstHtml.write(line)
                        dstHtml.write('var data = "')
                        for dl in data:
                            dstHtml.write(dl[:-1])
                        dstHtml.write('";\n')
                    elif '<input' in line:
                        pass
                    else:
                        dstHtml.write(line)

    os.remove(tmpData)


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


def get_volume_piecewise(view):
    renderer = view.GetClientSideObject().GetRenderer()
    for volume in renderer.GetVolumes():
        if volume.GetClassName() == 'vtkVolume':
            return volume.GetProperty().GetScalarOpacity()
    return None


def get_contour():
    for key, value in simple.GetSources().iteritems():
        if 'FlyingEdges' in key[0]:
            return value
    return None

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
# Image based Volume exploration
# -----------------------------------------------------------------------------


def export_volume_exploration_images(destinationPath, camera):
    view = simple.GetRenderView()
    pvw = get_volume_piecewise(view)
    maxOpacity = 0.5
    nbSteps = 10
    step = 250.0 / float(nbSteps)
    span = step * 0.4
    values = [float(v + 1) * step for v in range(0, nbSteps)]
    if pvw:
        idb = ImageDataSetBuilder(destinationPath, 'image/jpg', camera)
        idb.getDataHandler().registerArgument(priority=1, name='volume',
                                              values=values, ui='slider',
                                              loop='reverse')
        idb.start(view)
        for volume in idb.getDataHandler().volume:
            pvw.RemoveAllPoints()
            pvw.AddPoint(float(volume) - span, 0)
            pvw.AddPoint(float(volume), maxOpacity)
            pvw.AddPoint(float(volume) + span, 0)
            pvw.AddPoint(255, 0)
            idb.writeImages()
        idb.stop()

# -----------------------------------------------------------------------------
# Image based Contour exploration
# -----------------------------------------------------------------------------


def export_contour_exploration_images(destinationPath, camera):
    view = simple.GetRenderView()
    contour = get_contour()
    nbSteps = 10
    step = 250.0 / float(nbSteps)
    values = [float(v + 1) * step for v in range(0, nbSteps)]
    if contour:
        idb = ImageDataSetBuilder(destinationPath, 'image/jpg', camera)
        idb.getDataHandler().registerArgument(priority=1, name='contour',
                                              values=values, ui='slider',
                                              loop='reverse')
        idb.start(view)
        for contourValue in idb.getDataHandler().contour:
            contour.Value = [contourValue]
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
        'light': ['intensity'],  # 'normal', intensity
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
