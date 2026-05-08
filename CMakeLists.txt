cmake_minimum_required(VERSION 3.21)

project(kdeftpclient VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

qt_add_executable(kdeftpclient
    src/main.cpp
    src/app/SiteProfileService.h
    src/app/SiteProfileService.cpp
    src/app/TransferQueueService.h
    src/app/TransferQueueService.cpp
    src/infra/settings/InMemorySiteProfileRepository.h
    src/infra/settings/InMemorySiteProfileRepository.cpp
    src/infra/settings/JsonSiteProfileRepository.h
    src/infra/settings/JsonSiteProfileRepository.cpp
    src/ui/mainwindow.h
    src/ui/mainwindow.cpp
    src/ui/mainwindow.ui
    src/ui/dialogs/SiteManagerDialog.h
    src/ui/dialogs/SiteManagerDialog.cpp
    src/ui/dialogs/SiteProfileDialog.h
    src/ui/dialogs/SiteProfileDialog.cpp
    src/ui/widgets/TransferQueueWidget.h
    src/ui/widgets/TransferQueueWidget.cpp
)

target_include_directories(kdeftpclient PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(kdeftpclient PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
