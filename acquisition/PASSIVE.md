# Introduction

An experimental feature adds the ability to passively watch a directory for new
files in order to facilitate a passive mode of acquisition where another program
is acquiring data from the microscope. The Tomviz acquisition interface will
watch files in a specified directory using a regular expression to match the
file name.

The process watching the directory is a small, self-contained Python server with
a simple programming interface that can be called over the network. The process
should be started on a machine with access to files as they are acquired, this
can then be used from the Tomviz user interface.

# Installing the Acquisition Server

Get a copy of the Tomviz source repository, and ensure the machine has Python
available. You can install everything in a Python virtual environment, or add
the dependencies globally if preferred (in which case you will need elevated
prvileges).

    git clone --recursive git://github.com/openchemistry/tomviz.git
    cd tomviz/acquisition
    mkvirtualenv tomviz
    pip install 'git+https://cjh1@bitbucket.org/cjh1/pydm3reader.git@filelike'
    pip install https://github.com/bottlepy/bottle/archive/41ed6965.zip
    pip install -e .
    pip install -e .[tiff]
    pip install -e .[test]

At this point you will have a Python environment with the required Python tools.

# Starting the Acquisition Server

Once everything is installed you can start the acquisition server with the
following commands.

    workon tomviz
    tomviz-acquisition -a tomviz.acquisition.vendors.passive.PassiveWatchSource

This will result in a process running in the terminal on the machine that has a
directory to be watched passively.

# Connecting from the Application

Start the Tomviz application, click on the 'Tools' menu, and then select
'Acquisition'. This will open a dialog, from there click on 'Introspect' at the
bottom, and fill in the details. Typical testing parameters might be the default
host name of 'localhost', port number of '8080', watch path of '/tmp/test' and
the file name regex of '.*'.

Once ready click on 'Connect', and 'Watch' to begin observing the directory. A
preview of the last image to be acquired will be shown, and the pipeline will
get a 'Live' data source. This will be appended to, and the pipeline reexecuted
when new images are available.

# Starting a Test Sequence

We don't have any microscopes, and many of these features can be simulated using
image stacks already acquired. We have made a couple available in the testing
interface to aid in development. The following (from within the 'tomviz' Python
virtual environment) will cause an image to be written from a stack every five
seconds:

    tomviz-tiltseries-writer -p /tmp/test -d 5 -t tiff

It supports type of 'dm3' too, the path, delay, and type can all be modified. At
present the angle information is not being fed through, this will be added next.

# Active Development

None of these interfaces are set in stone, they are being developed to aid in
the extension of the pipeline to support use cases where data is being actively
acquired.
