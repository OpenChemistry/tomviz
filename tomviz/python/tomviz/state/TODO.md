# TODO/Notes

- We currently only have one way syncing from the Python console to the GUI.
  When changes are made in the GUI they should be synced back to the Python
  console state, when possible. We will have to add some sort of checking to the
  methods of the python objects to check whether their state is still valid. For
  state, if an operator is deleted in the UI then accessing it in the console
  should cause an exception to be raised.
- We are only dealing with the replace case for operators ( modify ). We need to
  support 'remove' and modules etc.
- We should think about modifying the deserialize(...) method so they will accept
  a sub set the other parameters that have changed.
- How do we deal with view, colormap etc.?
- The code is very much a prototype!
- We want to make sure that the tomviz.state module is clean ( hidde all
  internal attributes).
- Split state/__init.py  into _schemata, _models and _utils?
- Refactor model class (DataSource, Operator etc. ) to have common base class
  that deals with attributes.
- Add support for updating and adding DataSources.
- Add support for removing operators, modules and datasources.
- Add support for screenshoting view?
- Split ModuleManagerWrapper.cxx into a couple of appropriately named files.
  It would be nice if we had _wrapping.operators and _wrapping.state or
  something similar.
- Use description for doc string when generating classes if ParaView console can
  use it.
- Locking python console is not preventing timeing issus async vs sync. As in the
  main thread is not waiting for pipeline thread.
- Operator name generation still needs some work, we need to apply camelCasing to
  the case when we are using the label rather than the name.
- Is state the right subpackage? Change sync to execute(...)
- Add global pause functionality that will work for loading as well.
- Add ablilty to save pipeline state.
- Clean up todo's that need use to lookup the correct view.
- Finish filling out DataSource schema (colorMap etc ...)
- Operator attributes shouldn't be unnested they should remain under 'arguments'
- Enable check modified all, improve logic to work out what should be marked
  as modified, especially when we have operators added as well as modified ...
- Add syntatic sugar, such as state.pipeline[0].add_operator(op)
- Deal with updates to id ( i.e. we want to ignore them as they are a delete and replace)
- Move wrapping into namespace