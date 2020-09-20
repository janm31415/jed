if (WIN32)

set(SDL_INCLUDE_DIR "C:/Program Files/SDL2/include/SDL2/")
set(SDL_LIBRARIES "C:/Program Files/SDL2/lib/SDL2.lib" "C:/Program Files/SDL2/lib/SDL2main.lib")

set(SDL_TTF_INCLUDE_DIR "C:/Program Files/SDL_ttf/include/SDL2")
set(SDL_TTF_LIBRARIES "C:/Program Files/SDL_ttf/lib/SDL2_ttf.lib" "C:/Program Files/SDL_ttf/lib/freetype248.lib")

endif (WIN32)

if (UNIX)
  set(SDL_INCLUDE_DIR "/usr/include/SDL2/")
  set(SDL_LIBRARIES "/usr/lib/x86_64-linux-gnu/libSDL2-2.0.so" "/usr/lib/x86_64-linux-gnu/libSDL2-2.0.so.0")
  set(SDL_TTF_INCLUDE_DIR "/usr/include/SDL2/")
  set(SDL_TTF_LIBRARIES "/usr/lib/x86_64-linux-gnu/libSDL2_ttf-2.0.so" "/usr/lib/x86_64-linux-gnu/libSDL2_ttf-2.0.so.0")
endif (UNIX)

if (APPLE)
  set(SDL_INCLUDE_DIR "/Library/Frameworks/SDL2.framework/Headers/")
  set(SDL_LIBRARIES "/Library/Frameworks/SDL2.framework/SDL2")
  set(SDL_TTF_INCLUDE_DIR "/Library/Frameworks/SDL2_ttf.framework/Headers/")
  set(SDL_TTF_LIBRARIES "/Library/Frameworks/SDL2_ttf.framework/SDL2_ttf")
endif (APPLE)

set(HDRS
active_folder.h
buffer.h
clipboard.h
colors.h
engine.h
keyboard.h
mouse.h
pdcex.h
pref_file.h
process.h
settings.h
utils.h
    )
	
set(SRCS
buffer.cpp
clipboard.cpp
colors.cpp
engine.cpp
keyboard.cpp
main.cpp
mouse.cpp
pdcex.cpp
pref_file.cpp
settings.cpp
utils.cpp
)

set(JSON
${CMAKE_CURRENT_SOURCE_DIR}/../json/json.hpp
)

if (WIN32)
set(CMAKE_C_FLAGS_DEBUG "/W4 /MP /GF /RTCu /Od /MDd /Zi")
set(CMAKE_CXX_FLAGS_DEBUG "/W4 /MP /GF /RTCu /Od /MDd /Zi")
set(CMAKE_C_FLAGS_RELEASE "/W4 /MP /GF /O2 /Ob2 /Oi /Ot /MD /Zi")
set(CMAKE_CXX_FLAGS_RELEASE "/W4 /MP /GF /O2 /Ob2 /Oi /Ot /MD /Zi")
endif (WIN32)

# general build definitions
add_definitions(-D_SCL_SECURE_NO_WARNINGS)
add_definitions(-D_CRT_SECURE_NO_WARNINGS)

if (WIN32)
add_definitions(-DPDC_DLL_BUILD)
endif (WIN32)
add_definitions(-DPDC_RGB)
add_definitions(-DPDC_FORCE_UTF8)
add_definitions(-DPDC_WIDE)

if (WIN32)
add_executable(jed WIN32 ${HDRS} ${SRCS} ${JSON})
endif (WIN32)

if (UNIX)
add_executable(jed ${HDRS} ${SRCS} ${JSON})
endif (UNIX)

source_group("Header Files" FILES ${HDRS})
source_group("Source Files" FILES ${SRCS})
source_group("ThirdParty/json" FILES ${JSON})

 target_include_directories(jed
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../
    ${CMAKE_CURRENT_SOURCE_DIR}/../cpp-rrb/
    ${CMAKE_CURRENT_SOURCE_DIR}/../json/
    ${CMAKE_CURRENT_SOURCE_DIR}/../jtk/
    ${CMAKE_CURRENT_SOURCE_DIR}/../pdcurses/     
    ${SDL_INCLUDE_DIR}
    ${SDL_TTF_INCLUDE_DIR}
    )	
	
target_link_libraries(jed
    PRIVATE 
    pdcurses
    ${SDL_LIBRARIES}
    ${SDL_TTF_LIBRARIES}
    )	

add_custom_command(TARGET jed POST_BUILD 
   COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/Help.txt" "$<TARGET_FILE_DIR:jed>/Help.txt")