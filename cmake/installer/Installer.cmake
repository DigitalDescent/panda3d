# Filename: Installer.cmake
#
# Description: Defines the "installer" custom target, which builds a
#   platform-specific installer package:
#
#     Windows  – NSIS installer (.exe)
#     Linux    – DEB or RPM package (via CPack)
#     macOS    – DMG disk image with multi-package .mpkg
#     FreeBSD  – pkg package
#
# Usage: include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/installer/Installer.cmake")
#        Then: cmake --build <dir> --target installer

set(_INSTALLER_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(_PANDA_MAJOR_VER "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")

# ─── System-integration install rules (Linux / FreeBSD) ─────────────────────

if(UNIX AND NOT APPLE)
  # Man pages
  if(IS_DIRECTORY "${PROJECT_SOURCE_DIR}/doc/man")
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/doc/man/"
      DESTINATION "${CMAKE_INSTALL_MANDIR}/man1"
      COMPONENT Tools
      FILES_MATCHING PATTERN "*.1")
  endif()

  # Desktop files (only when tools are built)
  if(BUILD_TOOLS)
    install(FILES
      "${_INSTALLER_DIR}/pview.desktop"
      "${_INSTALLER_DIR}/pstats.desktop"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
      COMPONENT Tools)
  endif()

  # Freedesktop MIME type associations
  install(FILES "${_INSTALLER_DIR}/panda3d.xml"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/mime/packages"
    COMPONENT Core)
endif()

# ═════════════════════════════════════════════════════════════════════════════
#   Windows: NSIS
# ═════════════════════════════════════════════════════════════════════════════

if(WIN32)

  # Look for makensis in the thirdparty tree, then on the system PATH.
  if(THIRDPARTY_DIRECTORY AND EXISTS "${THIRDPARTY_DIRECTORY}/win-nsis/makensis.exe")
    set(MAKENSIS_EXECUTABLE "${THIRDPARTY_DIRECTORY}/win-nsis/makensis.exe")
  else()
    find_program(MAKENSIS_EXECUTABLE makensis)
  endif()

  if(NOT MAKENSIS_EXECUTABLE)
    message(STATUS "makensis not found -- 'installer' target will not be available")
    return()
  endif()

  message(STATUS "Found makensis: ${MAKENSIS_EXECUTABLE}")

  # Architecture.
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_NSIS_REGVIEW "64")
    set(_NSIS_ARCH_SUFFIX "-x64")
  else()
    set(_NSIS_REGVIEW "32")
    set(_NSIS_ARCH_SUFFIX "")
  endif()

  # Default install location that the installer proposes to the end-user.
  set(_NSIS_INSTALLDIR "C:\\Panda3D-${PROJECT_VERSION}${_NSIS_ARCH_SUFFIX}")

  # Build output directory.  For multi-config generators this includes
  # the configuration name; for single-config it points at the build root.
  if(IS_MULTICONFIG)
    set(_NSIS_BUILT "${PROJECT_BINARY_DIR}/$<CONFIG>")
  else()
    set(_NSIS_BUILT "${PROJECT_BINARY_DIR}")
  endif()

  # Output installer .exe goes next to the source tree.
  set(_NSIS_OUTFILE "${PROJECT_SOURCE_DIR}/Panda3D-${PROJECT_VERSION}${_NSIS_ARCH_SUFFIX}.exe")

  # Required defines.
  set(_NSIS_DEFS
    "/DCOMPRESSOR=lzma"
    "/DTITLE=Panda3D SDK ${PROJECT_VERSION}"
    "/DINSTALLDIR=${_NSIS_INSTALLDIR}"
    "/DOUTFILE=${_NSIS_OUTFILE}"
    "/DBUILT=${_NSIS_BUILT}"
    "/DSOURCE=${PROJECT_SOURCE_DIR}"
    "/DREGVIEW=${_NSIS_REGVIEW}"
    "/DMAJOR_VER=${_PANDA_MAJOR_VER}"
  )

  # If Python is bundled, tell NSIS which version.
  if(HAVE_PYTHON AND PYTHON_VERSION_STRING)
    set(_pyver "${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}")
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
      string(APPEND _pyver "-32")
    endif()
    list(APPEND _NSIS_DEFS "/DINCLUDE_PYVER=${_pyver}")
  endif()

  add_custom_target(installer
    COMMAND "${MAKENSIS_EXECUTABLE}" /V2 ${_NSIS_DEFS}
      "${_INSTALLER_DIR}/installer.nsi"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Building NSIS installer..."
    VERBATIM
  )

# ═════════════════════════════════════════════════════════════════════════════
#   macOS: DMG with multi-package .mpkg
# ═════════════════════════════════════════════════════════════════════════════

elseif(APPLE)

  find_program(PKGBUILD_EXECUTABLE pkgbuild)
  find_program(HDIUTIL_EXECUTABLE hdiutil)

  if(NOT PKGBUILD_EXECUTABLE OR NOT HDIUTIL_EXECUTABLE)
    message(STATUS "pkgbuild or hdiutil not found -- 'installer' target will not be available")
    return()
  endif()

  set(_DMG_INSTALL_DIR "/Library/Developer/Panda3D")

  # Build output directory (build tree, not install tree).
  if(IS_MULTICONFIG)
    set(_DMG_OUTPUT_DIR "${PROJECT_BINARY_DIR}/$<CONFIG>")
  else()
    set(_DMG_OUTPUT_DIR "${PROJECT_BINARY_DIR}")
  endif()

  # Collect Python version info.
  set(_DMG_PYTHON_VERSIONS "")
  if(HAVE_PYTHON)
    set(_DMG_PYTHON_VERSIONS "${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}")
  endif()

  # Feature flags for the script.
  set(_DMG_HAVE_PYTHON "0")
  set(_DMG_HAVE_FFMPEG "0")
  set(_DMG_HAVE_FMODEX "0")
  if(HAVE_PYTHON)
    set(_DMG_HAVE_PYTHON "1")
  endif()
  if(HAVE_FFMPEG)
    set(_DMG_HAVE_FFMPEG "1")
  endif()
  if(HAVE_FMODEX)
    set(_DMG_HAVE_FMODEX "1")
  endif()

  # Configure the DMG build script at build time via file(GENERATE).
  configure_file(
    "${_INSTALLER_DIR}/build_dmg.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/build_dmg.sh.in"
    @ONLY)
  file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/build_dmg.sh"
    INPUT  "${CMAKE_CURRENT_BINARY_DIR}/build_dmg.sh.in")

  add_custom_target(installer
    COMMAND bash "${CMAKE_CURRENT_BINARY_DIR}/build_dmg.sh"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Building macOS DMG installer..."
    VERBATIM
  )

# ═════════════════════════════════════════════════════════════════════════════
#   FreeBSD: pkg
# ═════════════════════════════════════════════════════════════════════════════

elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")

  find_program(PKG_EXECUTABLE pkg)

  if(NOT PKG_EXECUTABLE)
    message(STATUS "pkg not found -- 'installer' target will not be available")
    return()
  endif()

  # Configure the pkg build script.
  configure_file(
    "${_INSTALLER_DIR}/build_freebsd_pkg.sh.in"
    "${CMAKE_CURRENT_BINARY_DIR}/build_freebsd_pkg.sh.in"
    @ONLY)
  file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/build_freebsd_pkg.sh"
    INPUT  "${CMAKE_CURRENT_BINARY_DIR}/build_freebsd_pkg.sh.in")

  add_custom_target(installer
    COMMAND sh "${CMAKE_CURRENT_BINARY_DIR}/build_freebsd_pkg.sh"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Building FreeBSD package..."
    VERBATIM
  )

# ═════════════════════════════════════════════════════════════════════════════
#   Linux: DEB or RPM via CPack
# ═════════════════════════════════════════════════════════════════════════════

elseif(UNIX)

  find_program(_DPKG_DEB dpkg-deb)
  find_program(_RPMBUILD rpmbuild)

  if(NOT _DPKG_DEB AND NOT _RPMBUILD)
    message(STATUS "Neither dpkg-deb nor rpmbuild found -- 'installer' target will not be available")
    return()
  endif()

  set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Panda3D free 3D engine SDK")
  set(CPACK_PACKAGE_VENDOR "Panda3D")

  if(_DPKG_DEB)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_NAME "panda3d${_PANDA_MAJOR_VER}")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "rdb <me@rdb.name>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "libdevel")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_PROVIDES "panda3d")
    set(CPACK_DEBIAN_PACKAGE_CONFLICTS "panda3d")
    set(CPACK_DEBIAN_PACKAGE_REPLACES "panda3d")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
      "Panda3D free 3D engine SDK
 Panda3D is a game engine which includes graphics, audio, I/O, collision
 detection, and other abilities relevant to the creation of 3D games.
 Panda3D is open source and free software under the revised BSD license,
 and can be used for both free and commercial game development at no
 financial cost.
 .
 This package contains the SDK for development with Panda3D.")

    # Post-install / post-remove: run ldconfig.
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/deb_postinst"
      "#!/bin/sh\nldconfig\n")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/deb_postrm"
      "#!/bin/sh\nldconfig\n")
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
      "${CMAKE_CURRENT_BINARY_DIR}/deb_postinst;${CMAKE_CURRENT_BINARY_DIR}/deb_postrm")

    if(HAVE_PYTHON)
      set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "python3, python3-tk")
      string(APPEND CPACK_DEBIAN_PACKAGE_PROVIDES ", python3-panda3d")
    endif()

    message(STATUS "CPack: DEB generator enabled (package: ${CPACK_DEBIAN_PACKAGE_NAME})")

  elseif(_RPMBUILD)
    set(CPACK_GENERATOR "RPM")
    set(CPACK_RPM_PACKAGE_NAME "panda3d")
    set(CPACK_RPM_PACKAGE_LICENSE "BSD License")
    set(CPACK_RPM_PACKAGE_GROUP "Development/Libraries")
    set(CPACK_RPM_PACKAGE_DESCRIPTION
      "Panda3D is a game engine which includes graphics, audio, I/O, collision \
detection, and other abilities relevant to the creation of 3D games. \
Panda3D is open source and free software under the revised BSD license, \
and can be used for both free and commercial game development at no \
financial cost.\n\nThis package contains the SDK for development with Panda3D.")
    set(CPACK_RPM_PACKAGE_AUTOREQ ON)

    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/rpm_postinst.sh"
      "/sbin/ldconfig\n")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/rpm_postrm.sh"
      "/sbin/ldconfig\n")
    set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE
      "${CMAKE_CURRENT_BINARY_DIR}/rpm_postinst.sh")
    set(CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE
      "${CMAKE_CURRENT_BINARY_DIR}/rpm_postrm.sh")

    message(STATUS "CPack: RPM generator enabled")
  endif()

  include(CPack)

  # Find the cpack executable (same directory as cmake).
  get_filename_component(_cmake_bin_dir "${CMAKE_COMMAND}" DIRECTORY)
  find_program(_cpack_cmd NAMES cpack HINTS "${_cmake_bin_dir}" NO_DEFAULT_PATH)
  if(NOT _cpack_cmd)
    find_program(_cpack_cmd NAMES cpack)
  endif()

  add_custom_target(installer
    COMMAND "${_cpack_cmd}" --config "${PROJECT_BINARY_DIR}/CPackConfig.cmake"
    WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
    COMMENT "Building ${CPACK_GENERATOR} package..."
    VERBATIM
  )

endif()
