#pragma once
#include <filesystem>
#include <fstream>
#include <string>
#include "adpcm.h"

class Wav
{
    private:
    std::ofstream out;
    AudioConverter audio;
    public:

    void WriteWav(fs::path fileName);
};