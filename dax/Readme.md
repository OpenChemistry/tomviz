
How to get the dax plugins for matviz into matviz

1. make a plugin build dir
2. run cmake inside the plugin build dir point to the dax subfolder of matviz
3. build the plugin(s)
4. copy the plugin libraries into :
   mac: matviz.app/Contents/plugins
   linux: matviz build dir subdirectory called plugins
5. run the matviz application with the command line argument: --enable-streaming
