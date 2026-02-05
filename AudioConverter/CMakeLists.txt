project(AudioConverter)

include_directories(./)
include_directories(../third-party/stb)

add_executable(
    AudioConverter
    main.cpp
    adpcm.h
    adpcm.cpp
    AudioTypes.h
)