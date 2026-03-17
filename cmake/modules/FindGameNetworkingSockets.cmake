# Distributed under the Panda3D BSD license.
#
# Find GameNetworkingSockets
#
# Set GNS_USE_STEAMWORKS=ON to build against the Steamworks SDK instead of
# the standalone open-source library.
#
# This module defines:
#   GameNetworkingSockets_FOUND
#   GameNetworkingSockets_INCLUDE_DIR
#   GameNetworkingSockets_LIBRARY
#   GameNetworkingSockets::GameNetworkingSockets  (imported target)
#

option(GNS_USE_STEAMWORKS "Build GNS against the Steamworks SDK instead of standalone" OFF)

if(GNS_USE_STEAMWORKS)
  # Steamworks SDK: headers live under public/, library is steam_api / steam_api64.
  find_path(GameNetworkingSockets_INCLUDE_DIR
    NAMES "steam/steamnetworkingsockets.h"
    PATH_SUFFIXES "public"
  )

  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    find_library(GameNetworkingSockets_LIBRARY
      NAMES "steam_api64"
      PATH_SUFFIXES "redistributable_bin/win64" "redistributable_bin/linux64"
    )
  else()
    find_library(GameNetworkingSockets_LIBRARY
      NAMES "steam_api"
      PATH_SUFFIXES "redistributable_bin" "redistributable_bin/linux32"
    )
  endif()
else()
  # Standalone open-source build.
  find_path(GameNetworkingSockets_INCLUDE_DIR
    NAMES "steam/steamnetworkingsockets.h"
    PATH_SUFFIXES "GameNetworkingSockets"
  )

  find_library(GameNetworkingSockets_LIBRARY
    NAMES "GameNetworkingSockets" "libGameNetworkingSockets"
  )
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GameNetworkingSockets
  DEFAULT_MSG
  GameNetworkingSockets_LIBRARY
  GameNetworkingSockets_INCLUDE_DIR
)

if(GameNetworkingSockets_FOUND AND NOT TARGET GameNetworkingSockets::GameNetworkingSockets)
  add_library(GameNetworkingSockets::GameNetworkingSockets UNKNOWN IMPORTED GLOBAL)
  set_target_properties(GameNetworkingSockets::GameNetworkingSockets PROPERTIES
    IMPORTED_LOCATION "${GameNetworkingSockets_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${GameNetworkingSockets_INCLUDE_DIR}"
  )
  if(GNS_USE_STEAMWORKS)
    set_property(TARGET GameNetworkingSockets::GameNetworkingSockets APPEND PROPERTY
      INTERFACE_COMPILE_DEFINITIONS "GNS_USE_STEAMWORKS")
  endif()
endif()

mark_as_advanced(GameNetworkingSockets_INCLUDE_DIR GameNetworkingSockets_LIBRARY)
