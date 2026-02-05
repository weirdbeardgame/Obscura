#include "wav.h"

#define SAMPLE_RATE 48000
#define BIT_DEPTH 16
#define TYPE_OF_FORMAT 1 // pcm
#define NUM_CHANNELS 2
#define BYTE_RATE (SAMPLE_RATE * BIT_DEPTH * NUM_CHANNELS / 8)

void Wav::WriteWav(fs::path fileName)
{
    fs::path file = fileName.replace_extension("wav");

    s16 ** audioBuffer = audio.Samples();

    out.open(fileName, std::ios::binary);

    out << "RIFF";
    //out << audio.BufferSize();
    out << "WAVE";
    out << "fmt ";
    out << BIT_DEPTH;
    out << TYPE_OF_FORMAT;
    out << NUM_CHANNELS;
    out << SAMPLE_RATE;
    out << BYTE_RATE;

    out << "data";
    out << audio.BufferSize();
    // Write L and R channels here
    
    out.write(file.c_str(), std::ios::binary);
    out.close();
}