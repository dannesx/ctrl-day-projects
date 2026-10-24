#include "../wav_format.h"
static uint8_t input[132000];
extern "C" {
uint8_t* wav_buffer() { return input; }
int wav_validate(unsigned size) {
  if (size > sizeof(input)) return 0;
  WavInfo info;
  return readWav(size, [size](size_t offset, uint8_t* out, size_t count) {
    if (offset > size || count > size-offset) return false;
    for (size_t i=0; i<count; ++i) out[i]=input[offset+i];
    return true;
  }, info);
}
}
