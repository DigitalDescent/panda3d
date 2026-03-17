########################################################################
##
## makepandacore — shared helpers for makewheel.py.
##
########################################################################

import configparser
import os
import platform
import subprocess
import sys

import locations

########################################################################
## Global state
########################################################################

THIRDPARTYBASE = None
THIRDPARTYDIR = None
OPTIMIZE = "3"
VERBOSE = False
TARGET = None
TARGET_ARCH = None

# Is the host 64-bit?
if sys.platform == 'darwin':
    host_64 = (sys.maxsize > 0x100000000)
else:
    host_64 = (platform.architecture()[0] == '64bit')

# SDK dict — populated by callers that need it (e.g. makewheel).
SDK = {}

########################################################################
## Terminal colours
########################################################################

HAVE_COLORS = False
SETF = ""
try:
    import curses
    curses.setupterm()
    SETF = curses.tigetstr("setf")
    if SETF is None:
        SETF = curses.tigetstr("setaf")
    assert SETF is not None
    HAVE_COLORS = sys.stdout.isatty()
except Exception:
    pass


def GetColor(color=None):
    if not HAVE_COLORS:
        return ""
    if color is not None:
        color = color.lower()

    if color == "blue":
        token = curses.tparm(SETF, 1)
    elif color == "green":
        token = curses.tparm(SETF, 2)
    elif color == "cyan":
        token = curses.tparm(SETF, 3)
    elif color == "red":
        token = curses.tparm(SETF, 4)
    elif color == "magenta":
        token = curses.tparm(SETF, 5)
    elif color == "yellow":
        token = curses.tparm(SETF, 6)
    else:
        token = curses.tparm(curses.tigetstr("sgr0"))
    return token.decode('ascii')


########################################################################
## Logging
########################################################################


def Warn(msg, extra=None):
    if extra is not None:
        print("%sWARNING:%s %s %s%s%s" % (
            GetColor("red"), GetColor(), msg,
            GetColor("green"), extra, GetColor()))
    else:
        print("%sWARNING:%s %s" % (GetColor("red"), GetColor(), msg))
    sys.stdout.flush()

########################################################################
## Platform helpers
########################################################################


def GetHost():
    """Returns the host platform."""
    if sys.platform == 'win32' or sys.platform == 'cygwin':
        return 'windows'
    elif sys.platform == 'darwin':
        return 'darwin'
    elif sys.platform.startswith('linux'):
        try:
            osname = subprocess.check_output(["uname", "-o"])
            if osname.strip().lower() == b'android':
                return 'android'
            return 'linux'
        except Exception:
            return 'linux'
    elif sys.platform.startswith('freebsd'):
        return 'freebsd'
    elif sys.platform == 'android':
        return 'android'
    else:
        raise RuntimeError('Unrecognized sys.platform: %s' % (sys.platform))


def GetHostArch():
    """Returns the host architecture."""
    target = GetTarget()
    if target == 'windows':
        return 'x64' if host_64 else 'x86'
    machine = platform.machine()
    if machine.startswith('armv7'):
        return 'armv7a'
    return machine


def GetTarget():
    """Returns the target platform (defaults to host)."""
    global TARGET
    if TARGET is None:
        TARGET = GetHost()
    return TARGET


def GetTargetArch():
    """Returns the target architecture (defaults to host arch)."""
    global TARGET_ARCH
    if TARGET_ARCH is None:
        TARGET_ARCH = GetHostArch()
    return TARGET_ARCH


def CrossCompiling():
    """Returns True if we're cross-compiling."""
    return GetTarget() != GetHost()


def GetStrip():
    if TARGET == 'android':
        return 'llvm-strip'
    return 'strip'

########################################################################
## Binary / tool location
########################################################################


def LocateBinary(binary):
    """Search the system PATH for the given binary."""
    if os.path.isfile(binary):
        return binary

    p = os.environ.get("PATH", os.defpath)
    pathList = p.split(os.pathsep)
    suffixes = ['']

    if GetHost() == 'windows':
        if not binary.lower().endswith('.exe') and not binary.lower().endswith('.bat'):
            suffixes = ['.exe', '.bat']
        pathList = ['.'] + pathList

    for path in pathList:
        binpath = os.path.join(os.path.expanduser(path), binary)
        for suffix in suffixes:
            if os.access(binpath + suffix, os.X_OK):
                return os.path.abspath(os.path.realpath(binpath + suffix))
    return None


########################################################################
## Thirdparty directory helpers
########################################################################


def GetThirdpartyBase():
    """Returns the location of the 'thirdparty' directory."""
    global THIRDPARTYBASE
    if THIRDPARTYBASE is not None:
        return THIRDPARTYBASE
    THIRDPARTYBASE = os.environ.get("MAKEPANDA_THIRDPARTY", "thirdparty")
    return THIRDPARTYBASE


def GetThirdpartyDir():
    """Returns the thirdparty directory for the target platform."""
    global THIRDPARTYDIR
    if THIRDPARTYDIR is not None:
        return THIRDPARTYDIR

    base = GetThirdpartyBase()
    target = GetTarget()
    target_arch = GetTargetArch()

    if target == 'windows':
        vc = str(SDK.get("MSVC_VERSION", (14,))[0])
        if target_arch == 'x64':
            THIRDPARTYDIR = base + "/win-libs-vc" + vc + "-x64/"
        else:
            THIRDPARTYDIR = base + "/win-libs-vc" + vc + "/"

    elif target == 'darwin':
        THIRDPARTYDIR = base + "/darwin-libs-a/"

    elif target == 'linux':
        if target_arch in ("aarch64", "arm64"):
            THIRDPARTYDIR = base + "/linux-libs-arm64/"
        elif target_arch.startswith("arm"):
            THIRDPARTYDIR = base + "/linux-libs-arm/"
        elif target_arch in ("x86_64", "amd64"):
            THIRDPARTYDIR = base + "/linux-libs-x64/"
        else:
            THIRDPARTYDIR = base + "/linux-libs-a/"

    elif target == 'freebsd':
        if target_arch in ("aarch64", "arm64"):
            THIRDPARTYDIR = base + "/freebsd-libs-arm64/"
        elif target_arch.startswith("arm"):
            THIRDPARTYDIR = base + "/freebsd-libs-arm/"
        elif target_arch in ("x86_64", "amd64"):
            THIRDPARTYDIR = base + "/freebsd-libs-x64/"
        else:
            THIRDPARTYDIR = base + "/freebsd-libs-a/"

    elif target == 'android':
        THIRDPARTYDIR = base + "/android-libs-%s/" % target_arch
        if target_arch == 'armv7a' and not os.path.isdir(THIRDPARTYDIR):
            THIRDPARTYDIR = base + "/android-libs-arm/"

    elif target == 'emscripten':
        THIRDPARTYDIR = base + "/emscripten-libs/"

    else:
        Warn("Unsupported platform:", target)
        return None

    if VERBOSE:
        print("Using thirdparty directory: %s" % THIRDPARTYDIR)
    return THIRDPARTYDIR

########################################################################
## Optimize / verbose accessors
########################################################################


def GetOptimize():
    return int(OPTIMIZE)


def GetVerbose():
    return VERBOSE


def SetVerbose(verbose):
    global VERBOSE
    VERBOSE = verbose

########################################################################
## Metadata / version helpers
########################################################################

cfg_parser = None


def GetMetadataValue(key):
    global cfg_parser
    if not cfg_parser:
        cfg_parser = configparser.ConfigParser()
        path = os.path.join(os.path.dirname(__file__), '..', 'setup.cfg')
        assert cfg_parser.read(path), "Could not read setup.cfg file."

    value = cfg_parser.get('metadata', key)
    if key == 'classifiers':
        value = value.strip().split('\n')
    return value

########################################################################
## Python extension suffix helpers (used by makewheel)
########################################################################

ANDROID_TRIPLE = None


def GetPythonABI():
    if not CrossCompiling():
        soabi = locations.get_config_var('SOABI')
        if soabi:
            return soabi

    soabi = 'cpython-%d%d' % (sys.version_info[:2])

    if sys.version_info >= (3, 13):
        gil_disabled = locations.get_config_var("Py_GIL_DISABLED")
        if gil_disabled and int(gil_disabled):
            return soabi + 't'

    return soabi


def GetExtensionSuffix():
    target = GetTarget()
    if target == 'windows':
        if GetOptimize() <= 2:
            dllext = '_d'
        else:
            dllext = ''
        gil_disabled = locations.get_config_var("Py_GIL_DISABLED")
        suffix = 't' if gil_disabled and int(gil_disabled) else ''
        if GetTargetArch() == 'x64':
            return dllext + '.cp%d%d%s-win_amd64.pyd' % (sys.version_info[0], sys.version_info[1], suffix)
        else:
            return dllext + '.cp%d%d%s-win32.pyd' % (sys.version_info[0], sys.version_info[1], suffix)
    elif target == 'emscripten':
        abi = GetPythonABI()
        arch = GetTargetArch()
        return '.{0}-{1}-emscripten.so'.format(abi, arch)
    elif target == 'android':
        abi = GetPythonABI()
        triple = ANDROID_TRIPLE.rstrip('0123456789')
        return '.{0}-{1}.so'.format(abi, triple)
    elif CrossCompiling():
        return '.{0}.so'.format(GetPythonABI())
    else:
        import _imp
        return _imp.extension_suffixes()[0]
