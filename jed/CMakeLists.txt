set(HDRS
buffer.h
clipboard.h
colors.h
engine.h
jedicon.h
keyboard.h
mouse.h
pdcex.h
pref_file.h
settings.h
syntax_highlight.h
utils.h
    )
	
set(SRCS
buffer.cpp
clipboard.cpp
colors.cpp
engine.cpp
jedicon.cpp
keyboard.cpp
main.cpp
mouse.cpp
pdcex.cpp
pref_file.cpp
settings.cpp
syntax_highlight.cpp
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
add_executable(jed WIN32 ${HDRS} ${SRCS} ${JSON} jed.rc resource.h)
endif (WIN32)

if (UNIX)
  if (APPLE)
    set(MACOSX_BUNDLE_ICON_FILE jed.icns )
    set(APP_ICON_MACOSX ${CMAKE_CURRENT_SOURCE_DIR}/jed.icns)
    set_source_files_properties(${APP_ICON_MACOSX} PROPERTIES
           MACOSX_PACKAGE_LOCATION "Resources")    
    set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO")
    set(CMAKE_XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS "--deep" CACHE STRING "Other Code Signing Flags" FORCE)
    add_executable(jed MACOSX_BUNDLE ${HDRS} ${SRCS} ${JSON} ${APP_ICON_MACOSX})
  else (APPLE)
    add_executable(jed ${HDRS} ${SRCS} ${JSON})
  endif (APPLE)
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
    ${CMAKE_CURRENT_SOURCE_DIR}/../SDL2/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../SDL2_ttf/
    )	
	
target_link_libraries(jed
    PRIVATE 
    pdcurses
    SDL2
    SDL2main
    SDL2_ttf
    )	

add_custom_command(TARGET jed POST_BUILD
   COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/Help.txt" "$<TARGET_FILE_DIR:jed>/Help.txt")

add_custom_command(TARGET jed POST_BUILD
   COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/jed_syntax.json" "$<TARGET_FILE_DIR:jed>/jed_syntax.json")
