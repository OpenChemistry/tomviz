/* global window document alert */
/* eslint-disable import/prefer-default-export */

// CSS loading ----------------------------------------------------------------

import 'font-awesome/css/font-awesome.css';
import 'normalize.css';
import 'babel-polyfill';

// Local import ---------------------------------------------------------------

import * as Factory  from './factory';
import viewerBuilder from './types';

// Dependencies injections ----------------------------------------------------

const iOS = /iPad|iPhone|iPod/.test(window.navigator.platform);

// Add class to body if iOS device --------------------------------------------

if (iOS) {
  document.querySelector('body').classList.add('is-ios-device');
}

// Expose viewer factory method -----------------------------------------------

export function load(url, container) {
  const pathItems = url.split('/');
  const basepath = `${pathItems.slice(0, pathItems.length - 1).join('/')}/`;

  Factory.getJSON(url, (error, data) => {
    viewerBuilder(basepath, data, {}, (viewer) => {
      if (!viewer) {
        /* eslint-disable no-alert */
        alert('The metadata format seems to be unsupported.');
        /* eslint-enable no-alert */
        return;
      }
      console.log(viewer);

      Factory.createUI(viewer, container);
    });
  });
}
