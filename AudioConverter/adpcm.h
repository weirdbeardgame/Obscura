#pragma once
#include "AudioTypes.h"
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

class AudioConverter
{
 private:
  std::ifstream fileStream;
  s16 *AdpcmSpuBuf[2];

  int sizeDecoded;

 public:
  bool Open(fs::path path);
  void FillBuffer(int size, s16 **dec_buf);
  void Close();

  s16 **Samples()
  {
    return AdpcmSpuBuf;
  }

  int BufferSize()
  {
    return sizeDecoded;
  }
};