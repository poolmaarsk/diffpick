# Install rules and the package_windows / package_appimage targets.

install(TARGETS diffpick RUNTIME DESTINATION bin)
install(FILES assets/diffpick-256.png
    DESTINATION share/icons/hicolor/256x256/apps
    RENAME diffpick.png)
install(FILES LICENSE README.md docs/QT-SOURCES.txt docs/THIRD-PARTY.txt
    DESTINATION share/doc/diffpick)

if(UNIX AND NOT APPLE)
    install(FILES packaging/linux/diffpick.desktop
        DESTINATION share/applications)
    find_program(BASH_EXECUTABLE bash REQUIRED)
    add_custom_target(package_appimage
        COMMAND "${BASH_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/package-appimage.sh"
            "${CMAKE_BINARY_DIR}" "${PROJECT_VERSION}"
        DEPENDS diffpick
        VERBATIM
        COMMENT "Creating diffpick Linux AppImage"
    )
endif()

if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt
        HINTS C:/msys64/ucrt64/bin REQUIRED)
    find_program(POWERSHELL_EXECUTABLE NAMES pwsh powershell REQUIRED)
    get_filename_component(QT_BIN_DIR "${WINDEPLOYQT_EXECUTABLE}" DIRECTORY)
    get_filename_component(QT_ROOT_DIR "${QT_BIN_DIR}" DIRECTORY)
    find_file(QT_LGPL_FILE
        NAMES LGPL-3.0-only.txt LGPL3.txt
        HINTS "${QT_ROOT_DIR}" "${QT_ROOT_DIR}/.."
        PATH_SUFFIXES share/licenses/qt6-base LICENSES licenses
        REQUIRED)

    add_custom_target(package_windows
        COMMAND "${POWERSHELL_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_SOURCE_DIR}/scripts/package-windows.ps1"
            -BuildDir "${CMAKE_BINARY_DIR}"
            -WindeployQt "${WINDEPLOYQT_EXECUTABLE}"
            -QtLicense "${QT_LGPL_FILE}"
        DEPENDS diffpick
        VERBATIM
        COMMENT "Creating portable diffpick Windows package"
    )
endif()
