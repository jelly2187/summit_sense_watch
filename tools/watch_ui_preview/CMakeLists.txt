cmake_minimum_required(VERSION 3.16)
project(watch_ui_preview C CXX)
set(CMAKE_C_STANDARD 99)
get_filename_component(REPO "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(LV_CONF_PATH "${CMAKE_CURRENT_LIST_DIR}/lv_conf.h" CACHE STRING "" FORCE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL ON CACHE BOOL "" FORCE)
add_subdirectory("${REPO}/managed_components/lvgl__lvgl" lvgl)
add_executable(watch_ui_preview preview.c
    "${REPO}/components/watch_ui/watch_pages.c"
    "${REPO}/components/watch_ui/fonts/watch_cn_16.c"
    "${REPO}/components/watch_ui/fonts/watch_cn_20.c")
target_include_directories(watch_ui_preview PRIVATE "${REPO}/components/watch_ui/include")
target_link_libraries(watch_ui_preview PRIVATE lvgl m)
