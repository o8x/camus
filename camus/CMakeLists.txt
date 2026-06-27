file(GLOB SOURCE_FILES
        src/*.h
        src/*.cpp
)

add_library(camus STATIC ${SOURCE_FILES})
target_link_libraries(camus PUBLIC common)
target_include_directories(camus PUBLIC src)

add_executable(Camus main.cpp)
target_link_libraries(Camus PRIVATE camus)
