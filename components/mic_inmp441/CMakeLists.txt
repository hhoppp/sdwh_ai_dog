# 声明组件名称和源文件
idf_component_register(
    SRCS "src/mic_inmp441.c"          # 驱动实现文件
    INCLUDE_DIRS "include"            # 头文件路径（对外暴露）
    REQUIRES driver                   # 依赖的 ESP-IDF 组件
)

# 可选：添加编译选项（如警告抑制）
# target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-parameter)