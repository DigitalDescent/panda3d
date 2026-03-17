#!/usr/bin/env python
########################################################################
#
# CMake-based build wrapper for Panda3D.
#
# This script accepts the same command-line arguments as the legacy
# makepanda build system but drives CMake under the hood.
#
# Usage:
#   makepanda.py --everything
#   makepanda.py --use-python --use-bullet --threads 8
#
########################################################################

import sys
if sys.version_info < (3, 8):
    print("This version of Python is not supported, use version 3.8 or higher.")
    exit(1)

import os
import re
import getopt
import shutil
import subprocess
import multiprocessing

########################################################################
#
# Package list — the same set the legacy makepanda recognised.
#
########################################################################

PACKAGES = [
    "PYTHON", "DIRECT", "GL", "GLES", "GLES2", "TINYDISPLAY",
    "NVIDIACG", "EGL", "EIGEN", "OPENAL", "FMODEX", "VORBIS", "OPUS",
    "FFMPEG", "SWSCALE", "SWRESAMPLE", "ODE", "BULLET", "PANDAPHYSICS",
    "SPEEDTREE", "ZLIB", "PNG", "JPEG", "TIFF", "OPENEXR", "SQUISH",
    "FCOLLADA", "ASSIMP", "EGG", "FREETYPE", "HARFBUZZ", "VRPN",
    "OPENSSL", "FFTW", "ARTOOLKIT", "OPENCV", "DIRECTCAM", "VISION",
    "GTK3", "MFC", "WX", "FLTK", "COCOA", "X11", "PANDATOOL", "PVIEW",
    "DEPLOYTOOLS", "SKEL", "PANDAFX", "PANDAPARTICLESYSTEM", "CONTRIB",
    "SSE2", "NEON", "MIMALLOC",
]

# Mapping from makepanda package name → CMake variable name.
# Packages listed here have a 1:1 HAVE_<X> mapping in CMake.
# Packages that don't correspond to a CMake option (they are handled
# by higher-level BUILD_* flags or have no CMake equivalent) are
# mapped to None.
PKG_TO_CMAKE = {
    "PYTHON":               "HAVE_PYTHON",
    "DIRECT":               None,           # controlled via BUILD_DIRECT
    "GL":                   "HAVE_GL",
    "GLES":                 "HAVE_GLES1",
    "GLES2":                "HAVE_GLES2",
    "TINYDISPLAY":          "HAVE_TINYDISPLAY",
    "NVIDIACG":             "HAVE_CG",
    "EGL":                  "HAVE_EGL",
    "EIGEN":                "HAVE_EIGEN",
    "OPENAL":               "HAVE_OPENAL",
    "FMODEX":               "HAVE_FMODEX",
    "VORBIS":               "HAVE_VORBIS",
    "OPUS":                 "HAVE_OPUS",
    "FFMPEG":               "HAVE_FFMPEG",
    "SWSCALE":              "HAVE_SWSCALE",
    "SWRESAMPLE":           "HAVE_SWRESAMPLE",
    "ODE":                  "HAVE_ODE",
    "BULLET":               "HAVE_BULLET",
    "PANDAPHYSICS":         None,           # always built if panda is built
    "SPEEDTREE":            "HAVE_SPEEDTREE",
    "ZLIB":                 "HAVE_ZLIB",
    "PNG":                  "HAVE_PNG",
    "JPEG":                 "HAVE_JPEG",
    "TIFF":                 "HAVE_TIFF",
    "OPENEXR":              "HAVE_OPENEXR",
    "SQUISH":               "HAVE_SQUISH",
    "FCOLLADA":             "HAVE_FCOLLADA",
    "ASSIMP":               "HAVE_ASSIMP",
    "EGG":                  "HAVE_EGG",
    "FREETYPE":             "HAVE_FREETYPE",
    "HARFBUZZ":             "HAVE_HARFBUZZ",
    "VRPN":                 "HAVE_VRPN",
    "OPENSSL":              "HAVE_OPENSSL",
    "FFTW":                 "HAVE_FFTW",
    "ARTOOLKIT":            "HAVE_ARTOOLKIT",
    "OPENCV":               "HAVE_OPENCV",
    "DIRECTCAM":            None,           # not a CMake option
    "VISION":               None,           # built when opencv/artoolkit are
    "GTK3":                 "HAVE_GTK3",
    "MFC":                  None,
    "WX":                   None,
    "FLTK":                 None,
    "COCOA":                "HAVE_COCOA",
    "X11":                  "HAVE_X11",
    "PANDATOOL":            None,           # controlled via BUILD_PANDATOOL
    "PVIEW":                None,           # controlled via BUILD_TOOLS
    "DEPLOYTOOLS":          None,           # always built with python
    "SKEL":                 None,
    "PANDAFX":              None,           # always built
    "PANDAPARTICLESYSTEM":  None,           # always built
    "CONTRIB":              None,           # controlled via BUILD_CONTRIB
    "SSE2":                 "LINMATH_ALIGN",
    "NEON":                 None,           # not a cmake variable
    "MIMALLOC":             "HAVE_MIMALLOC",
}

# Packages that map to BUILD_* flags instead of HAVE_* flags.
PKG_TO_BUILD = {
    "DIRECT":    "BUILD_DIRECT",
    "PANDATOOL":  "BUILD_PANDATOOL",
    "PVIEW":      "BUILD_TOOLS",
    "CONTRIB":    "BUILD_CONTRIB",
}

########################################################################
#
# Optimization level mapping.
#
# makepanda levels:
#   1 = debug      → CMake Debug
#   2 = dev        → CMake Standard  (custom: -O2 + debug info)
#   3 = release    → CMake Release
#   4 = max opt    → CMake RelWithDebInfo (or Release)
#
########################################################################

OPT_TO_BUILD_TYPE = {
    1: "Debug",
    2: "Standard",
    3: "Release",
    4: "RelWithDebInfo",
}

########################################################################
#
# Global state — populated by parseopts().
#
########################################################################

VERBOSE       = False
INSTALLER     = False
WHEEL         = False
RUNTESTS      = False
CLEAN         = False
THREADCOUNT   = 0
OPTIMIZE      = 3
VERSION       = None
DISTRIBUTOR   = None
OUTPUTDIR     = None
STATIC        = False
OVERRIDES     = []   # list of "VAR=VALUE" strings
UNIVERSAL     = False
TARGET        = None
TARGET_ARCH   = None
GIT_COMMIT    = None
WINDOWS_SDK   = None

# Per-package state: True = explicitly enabled, False = explicitly disabled,
# None = not specified (let CMake auto-detect).
PKG_STATE = {pkg: None for pkg in PACKAGES}

########################################################################
#
# Helpers
#
########################################################################

def error(msg):
    print(f"\nError: {msg}", file=sys.stderr)
    sys.exit(1)


def run(cmd, **kwargs):
    """Run a subprocess, streaming output live."""
    if VERBOSE:
        if isinstance(cmd, list):
            print("+ " + " ".join(cmd))
        else:
            print("+ " + cmd)
    result = subprocess.run(cmd, **kwargs)
    if result.returncode != 0:
        sys.exit(result.returncode)
    return result


def find_cmake():
    """Locate the cmake executable."""
    cmake = shutil.which("cmake")
    if cmake:
        return cmake

    # On Windows, try common install locations.
    if sys.platform == "win32":
        for prog in (os.environ.get("ProgramFiles", r"C:\Program Files"),
                     os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")):
            candidate = os.path.join(prog, "CMake", "bin", "cmake.exe")
            if os.path.isfile(candidate):
                return candidate

    error("Could not find cmake.  Please install CMake 3.13+ and make sure it is on your PATH.")


def detect_generator():
    """Pick an appropriate CMake generator for the current platform.

    Returns (generator_name_or_None, is_multi_config).
    When generator is None, CMake will auto-detect (e.g. VS on Windows).
    """
    if sys.platform != "win32":
        return None, False  # let CMake pick (usually Unix Makefiles or Ninja)

    # Prefer Ninja if available.
    if shutil.which("ninja"):
        return "Ninja", False

    # Otherwise let CMake pick the newest installed Visual Studio.
    return None, True


########################################################################
#
# Usage / argument parsing
#
########################################################################

def usage(problem=None):
    if problem:
        print(f"\nError parsing command-line input: {problem}")

    print("""
Makepanda generates a compiled copy of Panda3D.  Build output
lives under build/<Config>/ by default (e.g. build/Release/).
Command-line arguments are:

  --help            (print the help message you're reading now)
  --verbose         (print out more information)
  --clean           (delete build directory before building)
  --tests           (run the test suite)
  --installer       (build an installer)
  --wheel           (build a pip-installable .whl)
  --optimize X      (optimization level can be 1,2,3,4)
  --version X       (set the panda version number)
  --distributor X   (short string identifying the distributor of the build)
  --outputdir X     (install to the specified directory instead of using build/<Config>)
  --threads N       (number of parallel build jobs; 0 = auto-detect)
  --universal       (build universal binaries (macOS 11.0+ only))
  --override "O=V"  (override a CMake cache variable, e.g. "HAVE_NET=OFF")
  --static          (builds libraries for static linking)
  --target X        (cross-compilation target)
  --arch X          (target architecture for cross-compilation)
  --windows-sdk=X   (specify Windows SDK version)
  --git-commit=X    (git commit SHA-1 hash)
""")

    for pkg in PACKAGES:
        p = pkg.lower()
        print(f"  --use-{p:<9s}   --no-{p:<9s} (enable/disable use of {pkg})")

    print("""
  --nothing         (disable every third-party lib)
  --everything      (enable every third-party lib)

The simplest way to compile panda is to just type:

  makepanda --everything
""")
    sys.exit(1)


def parseopts(args):
    global VERBOSE, INSTALLER, WHEEL, RUNTESTS, CLEAN
    global THREADCOUNT, OPTIMIZE, VERSION, DISTRIBUTOR
    global OUTPUTDIR, STATIC, OVERRIDES, UNIVERSAL
    global TARGET, TARGET_ARCH, GIT_COMMIT
    global WINDOWS_SDK

    removedopts = [
        "use-touchinput", "no-touchinput", "no-awesomium", "no-directscripts",
        "no-carbon", "no-physx", "no-rocket", "host", "osxtarget=",
        "no-max6", "no-max7", "no-max8", "no-max9", "no-max2009",
        "no-max2010", "no-max2011", "no-max2012", "no-max2013", "no-max2014",
        "no-maya6", "no-maya65", "no-maya7", "no-maya8", "no-maya85",
        "no-maya2008", "no-maya2009", "no-maya2010", "no-maya2011",
        "no-maya2012", "no-maya2013", "no-maya20135", "no-maya2014",
        "no-maya2015", "no-maya2016", "no-maya20165", "no-maya2017",
        "no-maya2018", "no-maya2019", "no-maya2020", "no-maya2022",
        "lzma", "genman", "nocolor", "rtdist", "debversion=", "rpmversion=",
        "rpmrelease=", "p3dsuffix=", "rtdist-version=", "directx-sdk=",
        "use-icl", "no-copy-python", "msvc-version=",
    ]

    longopts = [
        "help", "distributor=", "verbose", "tests",
        "optimize=", "everything", "nothing", "installer", "wheel",
        "version=", "threads=", "outputdir=", "override=",
        "static", "clean",
        "windows-sdk=",
        "universal", "target=", "arch=", "git-commit=",
    ] + removedopts

    anything = False
    for pkg in PACKAGES:
        longopts.append("use-" + pkg.lower())
        longopts.append("no-" + pkg.lower())

    try:
        opts, extras = getopt.getopt(args, "", longopts)
    except getopt.GetoptError as exc:
        usage(str(exc))

    for option, value in opts:
        if option == "--help":
            usage()
        elif option == "--verbose":
            VERBOSE = True
        elif option == "--clean":
            CLEAN = True
        elif option == "--tests":
            RUNTESTS = True
        elif option == "--installer":
            INSTALLER = True
        elif option == "--wheel":
            WHEEL = True
        elif option == "--optimize":
            OPTIMIZE = int(value)
        elif option == "--version":
            m = re.match(r'^\d+\.\d+(\.\d+)+', value)
            if not m:
                usage("version requires at least three numeric components (X.Y.Z)")
            VERSION = m.group()
        elif option == "--distributor":
            DISTRIBUTOR = value.strip()
        elif option == "--outputdir":
            OUTPUTDIR = value.strip()
        elif option == "--threads":
            THREADCOUNT = int(value)
        elif option == "--override":
            OVERRIDES.append(value.strip())
        elif option == "--static":
            STATIC = True
        elif option == "--universal":
            UNIVERSAL = True
        elif option == "--target":
            TARGET = value.strip()
        elif option == "--arch":
            TARGET_ARCH = value.strip()
        elif option == "--git-commit":
            GIT_COMMIT = value
        elif option == "--windows-sdk":
            WINDOWS_SDK = value.strip()
        elif option == "--everything":
            # Leave all packages at None (auto-detect) — CMake will enable
            # whatever it can find, rather than erroring on missing packages.
            anything = True
        elif option == "--nothing":
            for pkg in PACKAGES:
                PKG_STATE[pkg] = False
            anything = True
        elif option[2:] in removedopts or option[2:] + '=' in removedopts:
            print(f"Warning: ignoring removed option {option}")
        else:
            matched = False
            for pkg in PACKAGES:
                if option == "--use-" + pkg.lower():
                    PKG_STATE[pkg] = True
                    anything = True
                    matched = True
                    break
                elif option == "--no-" + pkg.lower():
                    PKG_STATE[pkg] = False
                    anything = True
                    matched = True
                    break
            if not matched:
                usage(f"Unrecognised option: {option}")

    if not anything:
        usage("You should specify a list of packages to use or --everything.")

    if OPTIMIZE not in (1, 2, 3, 4):
        usage("Invalid setting for --optimize (must be 1, 2, 3, or 4)")

    if GIT_COMMIT is not None and not re.match(r"^[a-f0-9]{40}$", GIT_COMMIT):
        usage("Invalid SHA-1 hash given for --git-commit option!")




########################################################################
#
# Build the cmake define list from the parsed options.
#
########################################################################

def build_cmake_defines(source_dir):
    """Return a list of -D arguments for the cmake configure step."""

    defs = []

    def D(var, val):
        if isinstance(val, bool):
            val = "ON" if val else "OFF"
        defs.append(f"-D{var}={val}")

    # Build type / optimization.
    build_type = OPT_TO_BUILD_TYPE.get(OPTIMIZE, "Release")
    D("CMAKE_BUILD_TYPE", build_type)

    # Static vs shared.
    D("BUILD_SHARED_LIBS", not STATIC)

    # Distributor.
    if DISTRIBUTOR:
        D("PANDA_DISTRIBUTOR", DISTRIBUTOR)

    # Install prefix — only set when the user explicitly requests an output
    # directory.  Otherwise build output lives directly under build/<Config>.
    if OUTPUTDIR:
        D("CMAKE_INSTALL_PREFIX", os.path.abspath(OUTPUTDIR))

    # Thirdparty directory (auto-detected by CMake but be explicit).
    thirdparty = os.path.join(source_dir, "thirdparty")
    if os.path.isdir(thirdparty):
        D("THIRDPARTY_DIRECTORY", thirdparty)

    # Universal builds (macOS).
    if UNIVERSAL:
        D("CMAKE_OSX_ARCHITECTURES", "x86_64;arm64")
    elif TARGET_ARCH:
        D("CMAKE_OSX_ARCHITECTURES", TARGET_ARCH)

    # Package enable/disable flags.
    for pkg, state in PKG_STATE.items():
        if state is None:
            continue  # let CMake auto-detect

        # Handle BUILD_* style packages.
        if pkg in PKG_TO_BUILD:
            D(PKG_TO_BUILD[pkg], state)

        # Handle HAVE_* style packages.
        cmake_var = PKG_TO_CMAKE.get(pkg)
        if cmake_var:
            D(cmake_var, state)

        # NVIDIACG also controls CGGL and CGD3D9.
        if pkg == "NVIDIACG":
            D("HAVE_CGGL", state)
            D("HAVE_CGD3D9", state)

    # Extra overrides (--override "VAR=VALUE").
    for ov in OVERRIDES:
        if "=" in ov:
            defs.append(f"-D{ov}")
        else:
            print(f"Warning: ignoring malformed --override value: {ov}")

    # Git commit.
    if GIT_COMMIT:
        D("PANDA_GIT_COMMIT_STR", GIT_COMMIT)

    # Version override.
    if VERSION:
        D("PANDA_VERSION", VERSION)

    # Windows SDK.
    if WINDOWS_SDK:
        D("CMAKE_SYSTEM_VERSION", WINDOWS_SDK)

    return defs


########################################################################
#
# Main entry point
#
########################################################################

def main():
    parseopts(sys.argv[1:])

    source_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    build_dir = os.path.join(source_dir, "build")

    cmake = find_cmake()

    # --clean: wipe the build directory.
    if CLEAN and os.path.isdir(build_dir):
        print("Removing build directory ...")
        shutil.rmtree(build_dir)

    os.makedirs(build_dir, exist_ok=True)

    # ----------------------------------------------------------
    # Phase 1: Configure
    # ----------------------------------------------------------

    cmake_defs = build_cmake_defines(source_dir)

    configure_cmd = [cmake, "-S", source_dir, "-B", build_dir]

    generator, multi_config = detect_generator()
    if generator:
        configure_cmd += ["-G", generator]
    # For multi-config generators (Visual Studio), specify x64 architecture.
    if multi_config:
        configure_cmd += ["-A", "x64"]

    configure_cmd += cmake_defs

    config_name = OPT_TO_BUILD_TYPE.get(OPTIMIZE, "Release")

    print("-- Configuring with CMake ...")
    if VERBOSE:
        print(f"   Source dir : {source_dir}")
        print(f"   Build dir  : {build_dir}")
        print(f"   Generator  : {generator or '(default)'}")
        print(f"   Build type : {config_name}")
    run(configure_cmd)

    # ----------------------------------------------------------
    # Phase 2: Build
    # ----------------------------------------------------------

    jobs = THREADCOUNT if THREADCOUNT > 0 else multiprocessing.cpu_count()

    build_cmd = [cmake, "--build", build_dir]

    # For multi-config generators, specify the config.
    if multi_config:
        build_cmd += ["--config", config_name]

    build_cmd += ["--parallel", str(jobs)]

    if VERBOSE:
        build_cmd += ["--verbose"]

    print(f"-- Building with {jobs} parallel job(s) ...")
    run(build_cmd)

    # Determine the usable output directory.
    # For multi-config generators, build output goes to build/<Config>/.
    # For single-config generators, it goes directly to build/.
    if multi_config:
        output_dir = os.path.join(build_dir, config_name)
    else:
        output_dir = build_dir

    # ----------------------------------------------------------
    # Phase 3 (optional): Install to a separate directory
    # ----------------------------------------------------------

    if OUTPUTDIR:
        install_cmd = [cmake, "--install", build_dir]
        if multi_config:
            install_cmd += ["--config", config_name]
        print(f"-- Installing to {os.path.abspath(OUTPUTDIR)} ...")
        run(install_cmd)
        # When an explicit output dir is given, use that as the output root.
        output_dir = os.path.abspath(OUTPUTDIR)

    # ----------------------------------------------------------
    # Phase 4 (optional): Tests
    # ----------------------------------------------------------

    if RUNTESTS:
        print("-- Running tests ...")
        test_cmd = [cmake, "--build", build_dir, "--target", "RUN_TESTS"]
        if multi_config:
            test_cmd += ["--config", config_name]
        run(test_cmd)

    # ----------------------------------------------------------
    # Phase 5 (optional): Wheel
    # ----------------------------------------------------------

    if WHEEL:
        print("-- Building wheel ...")
        wheel_script = os.path.join(source_dir, "makepanda", "makewheel.py")
        if os.path.isfile(wheel_script):
            run([sys.executable, wheel_script])
        else:
            print("Warning: makewheel.py not found, skipping wheel build.")

    # ----------------------------------------------------------
    # Phase 6 (optional): Installer
    # ----------------------------------------------------------

    if INSTALLER:
        print("-- Building installer ...")
        installer_cmd = [cmake, "--build", build_dir, "--target", "installer"]
        if multi_config:
            installer_cmd += ["--config", config_name]
        run(installer_cmd)

    print("-- Build complete.")


if __name__ == "__main__":
    main()
