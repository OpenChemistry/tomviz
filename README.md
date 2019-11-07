![tomviz_logo]

[![Build Status](https://dev.azure.com/OpenChemistry/tomviz/_apis/build/status/OpenChemistry.tomviz?branchName=master)](https://dev.azure.com/OpenChemistry/tomviz/_build/latest?definitionId=1&branchName=master)

The [Tomviz project][tomviz] is developing a cross platform, open source
application for the processing, visualization, and analysis of 3D tomographic
data. It features a complete pipeline capable processing data from alignment,
reconstruction, and segmentation through to displaying, visualizing, and
interacting with 3D reconstructions of tomographic data. Many of the data
operators are available as editable Python scripts that can be modified in
the interface to experiment with different techniques. The pipeline can be
saved to disk, and a number of common file formats are supported for
importing and exporting data.

![tomviz_screenshot]

The Tomviz project was founded by [Marcus D. Hanwell][Hanwell] and
[Utkarsh Ayachit][Ayachit] at [Kitware][Kitware], [David A.
Muller][Muller] ([Cornell University][Cornell]), and
[Robert Hovden][Hovden] ([University of Michigan][Michigan]),
funded by DOE Office of Science contract DE-SC0011385.

Installing
----------
<img align="right" src="https://github.com/OpenChemistry/tomviz/blob/master/docs/images/animation_nponcarbon1_small.gif">

We recommend downloading the current [stable release](../../releases),
but also provide nightly binaries built by our [dashboards][Dashboard] for
Windows, macOS, and Linux.

__Windows__: Follow the installation instructions, double-click on the Tomviz
icon to launch the application. __macOS__: After downloading the package
double-click to begin installation. Drag the Tomviz icon into your Applications
directory – or anywhere else you would like to store it. Double-click on the
icon to open it, nightly builds will require right-clicking and selecting open.
__Linux__: A binary (tar.gz) is provided, or it can be built from source. See
[instructions for building](BUILDING.md) found in the BUILDING.md document.

A Quick Tutorial
----------
  1. Open a sample dataset by clicking “Sample Menu > Reconstruction ” at the
     top menubar.
  2. Create a 3D volumetric visualization by clicking “Visualization > Volume”
     at the top menubar.
  3. Interact with your volume in the center panel titled “RenderView”.

User Guide
----------
Start by watching this short video to [see Tomviz in action][tomviz_in_action].

Also [Tomviz user guide](/docs/TomvizBasicUserGuide.pdf) has more detailed
information to get started.

Publications using Tomviz
------------------------
- [A Simple Preparation Method for Full-Range Electron Tomography of Nanoparticles and Fine Powders, E. Padgett et al., Microsc. & Microanalys. (2017)](https://www.cambridge.org/core/journals/microscopy-and-microanalysis/article/simple-preparation-method-for-fullrange-electron-tomography-of-nanoparticles-and-fine-powders/918054E2B052A391B82467EB42FCB09C)
- [Physical Confinement Promoting Formation of Cu2O–Au Heterostructures with Au Nanoparticles Entrapped within Crystalline Cu2O Nanorods, E. Asenath-Smith et al., Chem. Mater. (2017)](http://pubs.acs.org/doi/abs/10.1021/acs.chemmater.6b03653)
- [Nanomaterial datasets to advance tomography in scanning transmission electron
  microscopy, B. Levin et al., Nature Scientific Data (2016)](http://www.nature.com/articles/sdata201641)
- [Graphene kirigami, M.K. Blees et al., Nature (2015)](http://www.nature.com/nature/journal/v524/n7564/full/nature14588.html)

Contributing
------------

Our project uses GitHub for code review, please fork the project and make a
pull request if you would like us to consider your patch for inclusion.

![Kitware, Inc.][KitwareLogo]

  [tomviz]: https://tomviz.org/ "The Tomviz project"
  [tomviz_logo]: https://github.com/OpenChemistry/tomviz/blob/master/tomviz/icons/tomvizfull.png "tomviz"
  [tomviz_screenshot]: https://github.com/OpenChemistry/tomviz/blob/master/docs/images/screencap_mac_wide_v0.6.0.gif "tomviz screenshot v0.6.0"
  [tomviz_in_action]: https://vimeo.com/189945022 "Tomviz in action"
  [Kitware]: http://kitware.com/ "Kitware, Inc."
  [KitwareLogo]: http://www.kitware.com/img/small_logo_over.png "Kitware"
  [Cornell]: http://www.aep.cornell.edu/
  [Michigan]: http://www.engin.umich.edu/
  [Hanwell]: http://www.kitware.com/company/team/hanwell.html
  [Ayachit]: http://www.kitware.com/company/team/ayachit.html
  [Muller]: http://muller.research.engineering.cornell.edu/
  [Hovden]: http://www.roberthovden.com/
  [Dashboard]: http://open.cdash.org/index.php?project=tomviz "tomviz dashboard"
