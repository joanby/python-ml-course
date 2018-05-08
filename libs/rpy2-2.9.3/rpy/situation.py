"""
This module is currently primarily intended to be used as a script.
It will print information about the rpy2's environment (Python version,
R version, rpy2 version, etc...).
"""

import sys, os, subprocess

def assert_python_version():
    if not (sys.version_info[0] >= 3 and sys.version_info[1] >= 3):
        raise RuntimeError(
            "Python >=3.3 is required to run rpy2")

def r_version_from_subprocess():
    try:
        tmp = subprocess.check_output(("R", "--version"))
    except Exception as exc:  # FileNotFoundError, WindowsError, etc
        return None
    r_version = tmp.decode('ascii', 'ignore').split(os.linesep)
    if r_version[0].startswith("WARNING"):
        r_version = r_version[1]
    else:
        r_version = r_version[0].strip()
    return r_version

        
def r_home_from_subprocess():
    """Return the R home directory from calling 'R RHOME'."""
    try:
        tmp = subprocess.check_output(("R", "RHOME"), universal_newlines=True)
    except Exception as exc:  # FileNotFoundError, WindowsError, etc
        return
    r_home = tmp.split(os.linesep)
    if r_home[0].startswith("WARNING"):
        r_home = r_home[1]
    else:
        r_home = r_home[0].strip()
    return r_home


def r_home_from_registry():
    """Return the R home directory from the Windows Registry."""
    try:
        import winreg
    except ImportError:
        import _winreg as winreg
    try:
        hkey = winreg.OpenKeyEx(winreg.HKEY_LOCAL_MACHINE,
                                "Software\\R-core\\R",
                                0, winreg.KEY_QUERY_VALUE)
        r_home = winreg.QueryValueEx(hkey, "InstallPath")[0]
        winreg.CloseKey(hkey)
    except Exception as exc:  # FileNotFoundError, WindowsError, etc
        return None
    if sys.version_info[0] == 2:
        r_home = r_home.encode(sys.getfilesystemencoding())
    return r_home

def get_r_home():
    r_home = os.environ.get("R_HOME")

    if not r_home:
        r_home = r_home_from_subprocess()
    if not r_home and sys.platform == 'win32':
        r_home = r_home_from_registry()
    return r_home

def _make_bold(text):
    return '%s%s%s' % ('\033[1m', text, '\033[0m')
         
def iter_info():

    yield _make_bold('Python version:')
    yield sys.version
    if not (sys.version_info[0] == 3 and sys.version_info[1] >= 5):
        yield "*** rpy2 is primarily designed for Python >= 3.5" 
    
    yield _make_bold("Looking for R's HOME:")

    r_home = os.environ.get("R_HOME")
    yield "    Environment variable R_HOME: %s" % r_home
    
    r_home = r_home_from_subprocess()
    yield "    Calling `R RHOME`: %s" % r_home
    
    if sys.platform == 'win32':
        r_home = r_home_from_registry()
    else:
        r_home = '*** Only available on Windows ***'
    yield "    InstallPath in the registry: %s" % r_home

    try:
        import rpy2.rinterface._rinterface
        r_version_build = rpy2.rinterface._rinterface.R_VERSION_BUILD
        r_version_build = '-'.join(str(x) for x in r_version_build)
    except Exception:
        r_version_build = '*** Error while importing rpy2.rinterface ***'

    yield _make_bold("R version:")
    yield "    In the PATH: %s" % r_version_from_subprocess()
    yield "    Used to build rpy2: %s" % r_version_build

    r_libs = os.environ.get("R_LIBS")
    yield _make_bold("Additional directories to load R packages from:")
    yield r_libs

if __name__ == '__main__':

    for row in iter_info():
        print(row)
