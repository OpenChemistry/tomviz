This directory bundle ArcticViewer for Tomviz usage.

Build
-----

To update and build the given viewer, just run the following commands:

```
$ npm install
$ npm run bundle
```

Then the following file should be dropped along the exported data

```
${tomviz_src}/web/www/Tomviz.html
```

Upload to data.kitware.com
--------------------------
The generated tomviz.html should be upload to data.kitware.com so it can be used in the packaging process. Upload the new file into the following [folder](https://data.kitware.com/#collection/58c031ad8d777f0aef5d78d4/folder/58c1a33f8d777f0aef5d791e) ( leave the other versions ). Then update ```tomviz_html_sha512``` [here](https://github.com/OpenChemistry/tomviz/blob/master/CMakeLists.txt), with the sha512 for the new file, this will be used to download the correct version of the file.
