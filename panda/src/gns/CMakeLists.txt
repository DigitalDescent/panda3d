if(NOT HAVE_GAMENETWORKINGSOCKETS)
  return()
endif()

set(P3GNS_HEADERS
  config_gns.h
  gnsConnection.h
  gnsConnectionListener.h
  gnsConnectionManager.h
  gnsConnectionReader.h
  gnsConnectionWriter.h
  gnsNetAddress.h
)

set(P3GNS_SOURCES
  config_gns.cxx
  gnsConnection.cxx
  gnsConnectionListener.cxx
  gnsConnectionManager.cxx
  gnsConnectionReader.cxx
  gnsConnectionWriter.cxx
  gnsNetAddress.cxx
)

add_library(p3gns ${P3GNS_HEADERS} ${P3GNS_SOURCES})
set_target_properties(p3gns PROPERTIES DEFINE_SYMBOL BUILDING_PANDA_GNS)
target_link_libraries(p3gns panda PKG::GAMENETWORKINGSOCKETS)
target_interrogate(p3gns ALL)

set_target_properties(p3gns PROPERTIES IGATE_MODULE panda3d.gns)

if(NOT BUILD_METALIBS)
  install(TARGETS p3gns
    EXPORT GNS COMPONENT GNS
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/panda3d
    ARCHIVE COMPONENT GNSDevel)
endif()
install(FILES ${P3GNS_HEADERS} COMPONENT GNSDevel DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/panda3d)
