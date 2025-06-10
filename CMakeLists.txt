cmake_minimum_required(VERSION 3.15)
project(Літак)

set(CMAKE_CXX_STANDARD 17)

# Настройки для Windows
if(WIN32)
    add_definitions(-DWIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows")
endif()

# Подключаем GDI+ и ComCtl32
find_package(Gdiplus REQUIRED)
find_package(ComCtl32 REQUIRED)

# Ресурсы
set(RESOURCES
    src/resources.rc
)

# Исходники
set(SOURCES
    src/main.cpp
)

# Собираем исполняемый файл
add_executable(Літак ${SOURCES} ${RESOURCES})

# Линкуем библиотеки
target_link_libraries(Літак PRIVATE
    Gdiplus::Gdiplus
    comctl32
)

# Копируем ресурсы в build directory
file(COPY assets DESTINATION ${CMAKE_CURRENT_BINARY_DIR})