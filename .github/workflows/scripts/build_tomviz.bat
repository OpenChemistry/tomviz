@echo off

mkdir tomviz-build
cd tomviz-build

REM FIXME: setting the zlib paths manually shouldn't be necessary forever.
REM Try removing it sometime
cmake -G"Ninja" ^
  -DCMAKE_BUILD_TYPE:STRING=RelWithDebInfo ^
  -DCMAKE_INSTALL_LIBDIR:STRING=lib ^
  -DTOMVIZ_USE_EXTERNAL_VTK:BOOL=ON ^
  -DENABLE_TESTING:BOOL=ON ^
  -DPython3_FIND_STRATEGY:STRING=LOCATION ^
  -DZLIB_LIBRARY=%CONDA_PREFIX%\Library\lib\zlib.lib ^
  -DZLIB_INCLUDE_DIR=%CONDA_PREFIX%\Library\include ^
  ..\tomviz
if %errorlevel% neq 0 exit /b %errorlevel%

ninja
if %errorlevel% neq 0 exit /b %errorlevel%
