@echo off

REM
REM CMake-based build wrapper for Panda3D.
REM
REM Locates a Python 3.8+ interpreter and runs makepanda.py,
REM which in turn drives the CMake build system.
REM

if not exist makepanda\makepanda.py goto :missing_script

REM Try thirdparty Python first (legacy layout), then system Python.
set thirdparty=thirdparty
if defined MAKEPANDA_THIRDPARTY set thirdparty=%MAKEPANDA_THIRDPARTY%

if %PROCESSOR_ARCHITECTURE% == AMD64 (
  set suffix=-x64
) else (
  set suffix=
)

set PYTHON_EXE=

REM Search thirdparty for Python 3.14 down to 3.8.
for %%V in (3.14 3.13 3.12 3.11 3.10 3.9 3.8) do (
  if exist "%thirdparty%\win-python%%V%suffix%\python.exe" (
    if not defined PYTHON_EXE set "PYTHON_EXE=%thirdparty%\win-python%%V%suffix%\python.exe"
  )
)

REM Fall back to Python on PATH.
if not defined PYTHON_EXE (
  where python >nul 2>&1
  if not errorlevel 1 set PYTHON_EXE=python
)

if not defined PYTHON_EXE goto :missing_python

"%PYTHON_EXE%" makepanda\makepanda.py %*
goto :done

:missing_script
  echo You need to change directory to the root of the panda source tree
  echo before invoking makepanda.
  goto :done

:missing_python
  echo Could not find a Python interpreter.  Please install Python 3.8+
  echo and make sure it is on your PATH, or provide a thirdparty directory
  echo containing a win-python3.X%suffix% subdirectory.
  goto :done

:done
