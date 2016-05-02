![tomviz_logo]

What is Volumetric / Tomographic Data?
------------

Volumetric data is any 3D array of values—such as a stack of black and white images. 3D volumetric visualization requires unique software tools distinguishable from the more familiar 3D visualization of wireframes (used in animated cinema) or molecular coordinates (crystallography). Unlike crystallographic and surface data, volumetric datasets grow rapidly with its characteristic length—a 1024x1024x1024 cube of 32-bit values is 4.29 gigabytes.

For tomographic data, each dimension is a spatial coordinate (x-y-z) and the intensity represents the density of material. A human skull has brighter (higher-intensity) values at points where the hard skull is located. With electron tomography, gold nanoparticles will appear brighter than silica nanoparticles. Because density values are provided at all positions, not just the surfaces, tomography reveals the entire internal structure of an object.

However, other types of volumetric data may have a dimension that is not spatial: in a hyperspectral map the third dimension is spectroscopic (x-y-wavelength); a movie uses space and time (x-y-time); and a tilt series contains images at different projection angles (x-y-angle). Although tomographic data is the most directly interpretable, tomviz is capable of visualizing any type of volumetric data.


  [tomviz_logo]: https://github.com/OpenChemistry/tomviz/blob/master/tomviz/icons/tomvizfull.png "tomviz"
