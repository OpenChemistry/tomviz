import os, shutil
from paraview import simple
from paraview.web.dataset_builder import *

# -----------------------------------------------------------------------------
# Main function
# -----------------------------------------------------------------------------

def web_export(destinationPath, exportType, deltaPhi, deltaTheta):
    dest = '%s/data' % destinationPath
    camera =  {'type': 'spherical', 'phi': range(0, 360, deltaPhi), 'theta': range(-deltaTheta, deltaTheta + 1, deltaTheta)}

    # Choose export mode:
    if exportType == 0: 
        export_images(dest, camera)

    if exportType == 1:
        export_layers(dest, camera)

    copy_viewer(destinationPath)
    
# -----------------------------------------------------------------------------
# Helpers 
# -----------------------------------------------------------------------------

def get_proxy(id):
    remoteObj = simple.servermanager.ActiveConnection.Session.GetRemoteObject(int(id))
    return simple.servermanager._getPyProxy(remoteObj)

def copy_viewer(destinationPath):
    # FIXME !!!
    # searchPath = os.path.realpath(os.path.join(os.path.dirname(os.path.realpath(__file__)), '../../..'))
    # print 'search from', searchPath
    # for root, dirs, files in os.walk(searchPath):
    #     print root
    #     if 'index.html' in files:
    #         print 'copy', root, 'to', destinationPath
    #         shutil.copytree(os.join(root, 'tomviz'), os.join(destinationPath, 'tomviz'))
    #         return
    pass

def add_scene_item(scene, name, proxy, view):
    hasNormal = False
    hasColor = False
    colors = {}
    representation = {}
    rep = simple.GetRepresentation(proxy, view)
    
    # Skip volume
    if rep.Visibility == 0 or rep.Representation == 'Volume':
        return

    for prop in ['Representation']:
        representation[prop] = rep.GetProperty(prop).GetData()
    
    pdInfo = proxy.GetPointDataInformation()
    numberOfPointArrays = pdInfo.GetNumberOfArrays()
    for idx in xrange(numberOfPointArrays):
        array = pdInfo.GetArray(idx)
        rangeValues = array.GetRange(-1)
        if array.Name == 'Normals':
            hasNormal = True
        if array.Name not in ['vtkValidPointMask', 'Normals']:
            hasColor = True
            if rangeValues[0] == rangeValues[1]:
                colors[array.Name] = { 'constant': rangeValues[0] }
            else:
                colors[array.Name] = { 'location': 'POINT_DATA', 'range': [i for i in rangeValues] }
    
    # Get information about cell data arrays
    cdInfo = proxy.GetCellDataInformation()
    numberOfCellArrays = cdInfo.GetNumberOfArrays()
    for idx in xrange(numberOfCellArrays):
        array = cdInfo.GetArray(idx)
        hasColor = True
        colors[array.Name] = { 'location': 'CELL_DATA', 'range': array.GetRange(-1) }

    # Make sure Normals are available if lighting by normals
    source = proxy
    if 'normal' in scene['light'] and rep.Representation != 'Outline':
        rep.Visibility = 0
        surface = simple.ExtractSurface(Input=proxy)
        surfaceWithNormals = simple.GenerateSurfaceNormals(Input=surface)
        source = surfaceWithNormals

    if not hasColor or rep.Representation == 'Outline':
        colors = { 'solid': { 'constant': 0 } }

    scene['scene'].append({ 'name': name, 'source': source, 'colors': colors, 'representation': representation })


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
        # rep = simple.GetRepresentation(value, view)
        # if rep.Visibility == 0:
        #     print "skip", key
        #     continue

        # print 'process', key

        # if 'Outline' in key[0]: # Outline
        #     sceneDescription['scene'].append({
        #         'name': 'Outline',
        #         'source': value,
        #         'colors': {
        #             'solid': { 'constant': 0 }
        #         }
        #     })
        # elif 'PassThrough' in key[0]: # Slice
        #     sceneDescription['scene'].append(extract_scene_item('Othogonal Slice', value))
        #     pass
        # elif 'FlyingEdges' in key[0]: # Contour
        #     sceneDescription['scene'].append(extract_scene_item('Contour', value))
        #     pass
        # elif 'TrivialProducer' in key[0]: # volume
        #     # sceneDescription['scene'].append(extract_scene_item('Volume', value))
        #     pass
        # elif 'ResampleWithDataset' in key[0]: # ?
        #     sceneDescription['scene'].append(extract_scene_item('Resample', value))
        #     pass
        # elif 'Threshold' in key[0]: # Threshold
        #     sceneDescription['scene'].append(extract_scene_item('Threshold', value))
        #     pass


    # Generate export
    dsb = CompositeDataSetBuilder(destinationPath, sceneDescription, camera, {}, {}, view)
    dsb.start()
    dsb.writeData()
    dsb.stop(compress=False)
