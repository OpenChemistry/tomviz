/* global XMLHttpRequest window */

import MagicLensImageBuilder     from 'paraviewweb/src/Rendering/Image/MagicLensImageBuilder';
import PixelOperatorImageBuilder from 'paraviewweb/src/Rendering/Image/PixelOperatorImageBuilder';

import GenericViewer             from 'paraviewweb/src/React/Viewers/ImageBuilderViewer';
import MultiViewerWidget         from 'paraviewweb/src/React/Viewers/MultiLayoutViewer';
import ViewerSelector            from 'paraviewweb/src/React/Widgets/ButtonSelectorWidget';

import contains                  from 'mout/src/array/contains';
import extractURL                from 'mout/src/queryString/parse';
import merge                     from 'mout/src/object/merge';

import React                     from 'react';
import ReactDOM                  from 'react-dom';

import viewerBuilder             from './types';

// React UI map
const ReactClassMap = {
  GenericViewer,
  MultiViewerWidget,
  ViewerSelector,
};

var buildViewerForEnsemble = false;

var configOverwrite = {};

// ----------------------------------------------------------------------------

export function getJSON(url, callback) {
  var xhr = new XMLHttpRequest();

  xhr.open('GET', url, true);
  // xhr.setRequestHeader('Access-Control-Allow-Origin', '*');
  xhr.responseType = 'text';

  xhr.onload = function onLoad(e) {
    if (this.status === 200 || this.status === 0) {
      callback(null, JSON.parse(xhr.response));
      return;
    }
    callback(new Error(e), xhr);
  };

  xhr.onerror = function onError(e) {
    callback(e, xhr);
  };

  xhr.send();
}

// ----------------------------------------------------------------------------

function createImageBuilderCallback(renderers, methodToCall) {
  return function callback(data, envelope) {
    var count = renderers.length;

    while (count) {
      count -= 1;
      const renderer = renderers[count];
      renderer[methodToCall](data);
    }
  };
}

// ----------------------------------------------------------------------------

function createQueryDataModelCallback(datasets, args) {
  return function callback(data, envelope) {
    var count = datasets.length;
    const argName = data.name;
    const argValue = data.value;
    const datasetToUpdate = [];

    if (args.indexOf(argName) !== -1) {
      while (count) {
        count -= 1;
        const dataset = datasets[count];
        if (dataset.getValue(argName) !== argValue) {
          dataset.setValue(argName, argValue);
          datasetToUpdate.push(dataset);
        }
      }
    }

    setImmediate(() => {
      while (datasetToUpdate.length) {
        datasetToUpdate.pop().fetchData();
      }
    });
  };
}

// ----------------------------------------------------------------------------

export function bindDatasets(bindConfig, renderers) {
  bindConfig.forEach((binding) => {
    const datasets = [];
    const rendererList = [];

    Object.keys(renderers).forEach((name) => {
      if (binding.datasets.indexOf(name) !== -1) {
        rendererList.push(renderers[name].builder || renderers[name].painter);
        if (renderers[name].builder) {
          datasets.push(renderers[name].builder.queryDataModel);
        }
      }
    });
    const callbackFn = createQueryDataModelCallback(datasets, binding.arguments);
    datasets.forEach((queryDataModel) => {
      queryDataModel.onStateChange(callbackFn);
    });

    // Bind image builder properties
    if (binding.other) {
      binding.other.forEach((rendererBinding) => {
        var rendererCallback = createImageBuilderCallback(rendererList, rendererBinding.setter);
        rendererList.forEach(renderer => renderer[rendererBinding.listener](rendererCallback));
      });
    }

    // Bind image builder models
    if (binding.bind) {
      binding.bind.forEach((modelName) => {
        const bindList = rendererList.map(i => i[modelName]).filter(i => !!i);
        if (bindList.length) {
          bindList[0].bind(bindList);
        }
      });
    }
  });
}

// ----------------------------------------------------------------------------

export function createOperators(operatorConfig, renderers) {
  operatorConfig.forEach((operator) => {
    // Create Pixel Operator
    var pixelOperator = new PixelOperatorImageBuilder(operator.operation, operator.datasets);
    pixelOperator.name = operator.name; // Used for pixel operator if any

    // Register PixelOperator
    renderers[operator.name] = {
      builder: pixelOperator,
      name: operator.name,
    };
  });

  // Bind each image builder after all have been created
  operatorConfig.forEach((operator) => {
    var pixelOperator = renderers[operator.name].builder;

    // Generic callback to push data into the PixelOperator
    function pushDataToPixelOperator(data, envelope) {
      pixelOperator.updateData(data.builder.name, data);
    }

    // Listen to all datasets that PixelOperator care about
    operator.datasets.forEach(name => renderers[name].builder.onImageReady(pushDataToPixelOperator));
  });
}

// ----------------------------------------------------------------------------

export function createViewer(url, callback) {
  const pathItems = url.split('/');
  const basepath = `${pathItems.slice(0, pathItems.length - 1).join('/')}/`;

  getJSON('/config.json', (configErr, config_) => {
    var config = Object.assign({}, config_, configOverwrite);
    if (configErr) {
      config = {};
    }

    // Merge config with URL parameters as well
    config = merge(config, extractURL(window.location.href));

    // Update configuration if we build for ensemble
    if (buildViewerForEnsemble) {
      config.ensemble = true;
    }

    getJSON(url, (error, data) => {
      const dataType = data.type;
      var queryDataModel = null;
      var ready = 0;

      if (contains(dataType, 'ensemble-dataset')) {
        const renderers = {};
        const expected = data.Ensemble.datasets.length;

        // Let the factory know to not build everything
        buildViewerForEnsemble = true;

        data.Ensemble.datasets.forEach((ds) => {
          createViewer(basepath + ds.data, (viewer) => {
            ready += 1;
            if (viewer.ui === 'ViewerSelector') {
              renderers[ds.name] = {
                builder: viewer.list[0].imageBuilder,
                name: ds.name,
                queryDataModel: viewer.list[0].queryDataModel,
              };
              viewer.list[0].imageBuilder.name = ds.name;
              while (viewer.list.length > 1) {
                viewer.list.pop().destroy();
              }
            } else {
              renderers[ds.name] = {
                builder: viewer.imageBuilder,
                name: ds.name,
                queryDataModel: viewer.queryDataModel,
              };
              viewer.imageBuilder.name = ds.name; // Used for pixel operator if any
            }

            if (!queryDataModel) {
              queryDataModel = viewer.queryDataModel;
            }

            if (ready === expected) {
              // Apply binding if any
              bindDatasets(data.Ensemble.binding || [], renderers);

              // Create pixel operators if any
              createOperators(data.Ensemble.operators || [], renderers);

              // Ready for UI deployment
              callback({ renderers, queryDataModel, ui: 'MultiViewerWidget' });
            }
          });
        });
      } else if (contains(dataType, 'arctic-viewer-list')) {
        // Show dataset listing
        callback({
          ui: 'ArcticListViewer',
          list: data.list,
          basePath: '/data/',
        });
      } else if (config.MagicLens && !config.ensemble) {
        viewerBuilder(basepath, data, config, (background) => {
          if (background.allowMagicLens) {
            viewerBuilder(basepath, data, config, (viewer) => {
              var defaultSynchList = ['phi', 'theta', 'n_pos', 'time'];
              background.imageBuilder.queryDataModel.link(viewer.imageBuilder.queryDataModel, defaultSynchList, true);
              viewer.imageBuilder.queryDataModel.link(background.imageBuilder.queryDataModel, defaultSynchList, true);

              viewer.imageBuilder = new MagicLensImageBuilder(viewer.imageBuilder, background.imageBuilder);
              callback(viewer);
            });
          } else {
            config.MagicLens = false;
            callback(background);
          }
        });
      } else if (viewerBuilder(basepath, data, config, callback)) {
        // We are good to go
      }
    });
  });
}

// ----------------------------------------------------------------------------

export function createUI(viewer, container, callback) {
  if (viewer.bgColor && viewer.ui !== 'MultiViewerWidget') {
    container.style[(viewer.bgColor.indexOf('gradient') !== -1) ? 'background' : 'background-color'] = viewer.bgColor;
  }

  // Make sure we trigger a render when the UI is mounted
  setImmediate(() => {
    var renderers = viewer.renderers || {};
    Object.keys(renderers).forEach((name) => {
      if (renderers[name].builder && renderers[name].builder.update) {
        renderers[name].builder.update();
      }
    });
    if (viewer.imageBuilder && viewer.imageBuilder.update) {
      viewer.imageBuilder.update();
    }
  });

  // Unmount any previously mounted React component
  ReactDOM.unmountComponentAtNode(container);

  if (viewer.ui === 'ReactComponent') {
    ReactDOM.render(viewer.component, container, callback);
  } else {
    ReactDOM.render(React.createElement(ReactClassMap[viewer.ui], viewer), container, callback);
  }
}

// ----------------------------------------------------------------------------

export function updateConfig(newConf = {}) {
  configOverwrite = newConf;
}
