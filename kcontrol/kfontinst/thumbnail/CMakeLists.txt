
include_directories(
    ${KDEBASE_WORKSPACE_SOURCE_DIR}/kcontrol/kfontinst/viewpart/
     )

set(fontthumbnail_PART_SRCS FontThumbnail.cpp )

kde4_add_plugin(fontthumbnail ${fontthumbnail_PART_SRCS})

target_link_libraries(fontthumbnail  ${KDE4_KIO_LIBS} kfontinstui kfontinst )

install(TARGETS fontthumbnail  DESTINATION ${PLUGIN_INSTALL_DIR} )
install( FILES fontthumbnail.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )

