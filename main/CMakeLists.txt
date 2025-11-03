set(src_dirs
    .
    ${CMAKE_SOURCE_DIR}/components/lvgl/demos/benchmark  # 添加 demo 源文件路径
    ${CMAKE_SOURCE_DIR}/components/lvgl/demos/benchmark/assets  # 添加 demo 头文件路径
    ${CMAKE_SOURCE_DIR}/components/lvgl/demos/widgets  # 添加 demo 头文件路径
)

set(include_dirs
    .
    ${CMAKE_SOURCE_DIR}/components/lvgl
    ${CMAKE_SOURCE_DIR}/components/lvgl/demos/benchmark  # 添加 demo 头文件路径
    ${CMAKE_SOURCE_DIR}/components/lvgl/demos/benchmark/assets  # 添加 demo 头文件路径
    ${CMAKE_SOURCE_DIR}/components/lvgl/demos/widgets  # 添加 demo 头文件路径
)

set(priv_requires
    driver
    esp_timer
    esp_driver_ledc
    co5300_driver
    CST820
    lvgl
)

idf_component_register(
    SRC_DIRS ${src_dirs}
    INCLUDE_DIRS ${include_dirs}
    PRIV_REQUIRES ${priv_requires}
)
