#! /usr/bin/env node
/* eslint-disable */

var path = require('path');
var shell = require('shelljs');
var fs = require('fs');

var templatePath = path.resolve(__dirname, './template.html');
var destPath = path.resolve(__dirname, '../www');
var outputFile = path.join(destPath, 'TomViz.html');

function bundleIntoSingleHtml() {
  var htmlTemplate = fs.readFileSync(templatePath, { encoding: 'utf8', flag: 'r' });
  var jsCode = fs.readFileSync(path.join(destPath, 'tomviz.js'), { encoding: 'utf8', flag: 'r' });

  var lines = htmlTemplate.split('\n');
  var count = lines.length;
  while (count--) {
    if (lines[count].indexOf('</body>') !== -1) {
      lines[count] = [
        '<script type="text/javascript">',
        jsCode,
        '</script>',
        '</body>',
      ].join('\n');
    }
  }

  shell.ShellString(lines.join('\n')).to(outputFile);
}

// Run bundle
bundleIntoSingleHtml();
