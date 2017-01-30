import QueryDataModel       from 'paraviewweb/src/IO/Core/QueryDataModel';
import LookupTableManager   from 'paraviewweb/src/Common/Core/LookupTableManager';
import contains             from 'mout/src/array/contains';

import ViewerB from 'arctic-viewer/lib/types/CompositeImageQueryDataModel';
import ViewerC from 'arctic-viewer/lib/types/CompositePipeline';
import ViewerE from 'arctic-viewer/lib/types/DepthComposite';
import ViewerF from 'arctic-viewer/lib/types/FloatImage';
import ViewerH from 'arctic-viewer/lib/types/ImageQueryDataModel';
import ViewerI from 'arctic-viewer/lib/types/SortedComposite';
import ViewerJ from 'arctic-viewer/lib/types/TimeFloatImage';

const dataViewers = [
  ViewerB,
  ViewerC,
  ViewerE,
  ViewerF,
  ViewerH,
  ViewerI,
  ViewerJ,
];

const lookupTableManager = new LookupTableManager();

export default function build(basepath, data, config, callback) {
  var foundViewer = false;
  var viewerCount = dataViewers.length;

  const dataType = data.type;
  const viewer = {
    ui: 'GenericViewer',
    config,
    allowMagicLens: true,
  };

  // Initializer shared variables
  config.lookupTableManager = lookupTableManager;

  // Update background if available
  if (data && data.metadata && data.metadata.backgroundColor) {
    viewer.bgColor = data.metadata.backgroundColor;
  }

  // Update QueryDataModel if needed
  if (contains(dataType, 'tonic-query-data-model')) {
    viewer.queryDataModel = config.queryDataModel || new QueryDataModel(data, basepath);
  }

  // Find the right viewer and build it
  const args = { basepath, data, callback, viewer, dataType };
  while (viewerCount && !foundViewer) {
    viewerCount -= 1;
    foundViewer = dataViewers[viewerCount](args);
  }

  setImmediate(() => callback(viewer));
  return foundViewer;
}
