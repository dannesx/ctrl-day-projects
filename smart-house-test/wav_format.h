#pragma once
#include <stdint.h>
#include <stddef.h>

struct WavInfo { uint32_t offset = 0, length = 0; };
inline uint32_t wavU32(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24; }
inline uint16_t wavU16(const uint8_t* p) { return uint16_t(p[0]) | uint16_t(p[1])<<8; }
inline bool wavTag(const uint8_t* p, const char* tag) { return p[0]==tag[0] && p[1]==tag[1] && p[2]==tag[2] && p[3]==tag[3]; }
// Bounds-checked RIFF traversal: accepts metadata chunks and odd-byte padding.
template<class Reader> bool readWav(size_t size, Reader read, WavInfo& info) {
  info = {};
  uint8_t header[16];
  if (size < 44 || !read(0, header, 12) || !wavTag(header,"RIFF") || !wavTag(header+8,"WAVE") || wavU32(header+4) != size-8) return false;
  bool format = false, data = false;
  size_t offset = 12;
  while (offset < size) {
    if (size-offset < 8 || !read(offset,header,8)) return false;
    uint32_t length = wavU32(header+4);
    bool fmt = wavTag(header,"fmt "), pcm = wavTag(header,"data");
    offset += 8;
    if (length > size-offset) return false;
    if (fmt) {
      if (format || length < 16 || !read(offset,header,16)) return false;
      if (wavU16(header)!=1 || wavU16(header+2)!=1 || wavU32(header+4)!=16000 ||
          wavU32(header+8)!=32000 || wavU16(header+12)!=2 || wavU16(header+14)!=16) return false;
      format = true;
    } else if (pcm) {
      if (data || !length || (length & 1)) return false;
      info.offset = offset; info.length = length; data = true;
    }
    offset += length;
    if (length & 1) { if (offset == size) return false; ++offset; }
  }
  return format && data;
}
