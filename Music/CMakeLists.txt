cmake_minimum_required(VERSION 3.16)

# 设置项目名称和版本
project(QVideoPlayer 
    VERSION 1.0.0 
    LANGUAGES CXX
)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 启用Qt的自动生成功能
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 查找Qt6依赖
find_package(Qt6 COMPONENTS 
    Core 
    Gui 
    Widgets 
    Multimedia 
    REQUIRED
)

# 定义FFmpeg根目录
set(FFMPEG_ROOT ${CMAKE_SOURCE_DIR}/ffmpeg-n4.4.4)

# 检查FFmpeg目录是否存在
if(NOT EXISTS ${FFMPEG_ROOT})
    message(FATAL_ERROR "FFmpeg directory not found at ${FFMPEG_ROOT}")
endif()

# 设置FFmpeg包含目录
set(FFMPEG_INCLUDE_DIRS ${FFMPEG_ROOT}/include)

# 设置FFmpeg库目录
set(FFMPEG_LIBRARY_DIRS ${FFMPEG_ROOT}/lib)

# 设置FFmpeg运行时库目录（用于复制DLL）
set(FFMPEG_BIN_DIR ${FFMPEG_ROOT}/bin)

# 设置源文件
set(SOURCES 
    main.cpp 
    videoplayer.cpp 
    ffmpegwrapper.cpp 
    app.rc
)

# 设置头文件
set(HEADERS 
    videoplayer.h 
    ffmpegwrapper.h 
)

# 设置UI文件
set(UIS 
    videoplayer.ui 
)

# 设置资源文件
set(RESOURCES 
    resources.qrc 
)

# 创建可执行文件
qt_add_executable(QVideoPlayer WIN32
    ${SOURCES} 
    ${HEADERS} 
    ${UIS} 
    ${RESOURCES} 
)

# 链接Qt库
target_link_libraries(QVideoPlayer PRIVATE 
    Qt6::Core 
    Qt6::Gui 
    Qt6::Widgets 
    Qt6::Multimedia 
)

# 链接FFmpeg库
# 简化FFmpeg库链接，直接指定库名称，让CMake自动查找
if(MSVC)
    # Windows下使用.lib文件
    target_link_libraries(QVideoPlayer PRIVATE 
        ${FFMPEG_LIBRARY_DIRS}/avcodec.lib 
        ${FFMPEG_LIBRARY_DIRS}/avformat.lib 
        ${FFMPEG_LIBRARY_DIRS}/avutil.lib 
        ${FFMPEG_LIBRARY_DIRS}/swscale.lib 
        ${FFMPEG_LIBRARY_DIRS}/swresample.lib 
        ${FFMPEG_LIBRARY_DIRS}/avdevice.lib 
    )
else()
    # 其他平台使用动态库
    target_link_libraries(QVideoPlayer PRIVATE 
        avcodec 
        avformat 
        avutil 
        swscale 
        swresample 
        avdevice 
    )
endif()

# 添加FFmpeg包含目录
target_include_directories(QVideoPlayer PRIVATE ${FFMPEG_INCLUDE_DIRS})

# 添加FFmpeg库目录
target_link_directories(QVideoPlayer PRIVATE ${FFMPEG_LIBRARY_DIRS})

# 设置输出目录
set_target_properties(QVideoPlayer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# 复制FFmpeg DLL到输出目录（仅Windows）
if(WIN32)
    add_custom_command(TARGET QVideoPlayer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${FFMPEG_BIN_DIR}
            $<TARGET_FILE_DIR:QVideoPlayer>
        COMMENT "Copying FFmpeg DLLs to output directory"
    )
endif()

# 简化编译选项，避免过于严格的检查
if(MSVC)
    # MSVC特定选项 - 简化版本
    target_compile_options(QVideoPlayer PRIVATE 
        /W3        # 警告级别3（降低要求）
        /MP        # 多处理器编译
    )
else()
    # GCC/Clang特定选项 - 简化版本
    target_compile_options(QVideoPlayer PRIVATE 
        -Wall      # 启用所有警告
    )
endif()

# 添加安装规则
install(TARGETS QVideoPlayer 
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# 安装FFmpeg DLLs（仅Windows）
if(WIN32)
    install(DIRECTORY ${FFMPEG_BIN_DIR}/ 
        DESTINATION bin
        FILES_MATCHING PATTERN "*.dll"
    )
endif()

# 提供更友好的错误信息，帮助用户解决编译器问题
if(NOT CMAKE_CXX_COMPILER AND MSVC)
    message(WARNING "\n==========================================")
    message(WARNING "No C++ compiler found!")
    message(WARNING "Please install Visual Studio with C++ development tools.")
    message(WARNING "Recommended: Visual Studio 2019 or later")
    message(WARNING "Make sure to select 'Desktop development with C++' workload")
    message(WARNING "==========================================\n")
elseif(NOT CMAKE_CXX_COMPILER)
    message(WARNING "\n==========================================")
    message(WARNING "No C++ compiler found!")
    message(WARNING "Please install a C++ compiler for your platform.")
    message(WARNING "For Windows: Visual Studio with C++ tools")
    message(WARNING "For Linux: gcc/g++ or clang/clang++")
    message(WARNING "For macOS: Xcode Command Line Tools")
    message(WARNING "==========================================\n")
endif()
