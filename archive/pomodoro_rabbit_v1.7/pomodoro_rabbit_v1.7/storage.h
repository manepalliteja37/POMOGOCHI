// storage.h - crash-safe save files on LittleFS (Stage 6a, goes into the final firmware as-is)
//
// Every "blob" (settings, pet, todos, epoch...) is a plain struct saved into TWO files:
//   /name.a  and  /name.b
// Each save goes to the OLDER slot with seq+1, a data version and a CRC32.
// Load picks the newest slot whose CRC is good. If the power dies mid-write, only the
// slot being written is broken - the other one still holds the previous good save.
//
// Data version: when firmware adds fields to a struct, bump its version. Old saves are
// still loaded (bytes copied over the defaults), so new fields keep their defaults.
//
// Use:
//   storeBegin();
//   Blob petBlob = { "pet", 1, &pet, sizeof(pet) };
//   storeLoad(petBlob);     // false = no valid save, struct keeps its defaults
//   storeSave(petBlob);     // only call when something changed
#pragma once
#include <Arduino.h>
#include <LittleFS.h>

#define STORE_MAGIC   0x4F4D4F54UL   // "TOMO"
#define STORE_MAX     512            // biggest blob in bytes

struct BlobHdr { uint32_t magic; uint16_t ver; uint16_t size; uint32_t seq; uint32_t crc; };
struct Blob {
  const char* name; uint16_t ver; void* data; uint16_t size;
  uint32_t seq = 0;        // seq of the newest good save
  int8_t   slot = -1;      // 0 = a, 1 = b, -1 = none yet
  uint8_t  badSlots = 0;   // slots found broken at load (a power cut caught mid-write)
  uint16_t loadedVer = 0;  // version of the data that was loaded
  uint32_t lastSaveUs = 0;
};

static uint8_t storeTmp[STORE_MAX] __attribute__((aligned(4)));

uint32_t storeCrc(const uint8_t* p, size_t n, uint32_t c = 0xFFFFFFFFUL) {
  while (n--) { c ^= *p++; for (uint8_t k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320UL & (0UL - (c & 1))); }
  return c;
}

uint32_t storeHdrCrc(const BlobHdr& h, const uint8_t* data) {
  uint32_t c = storeCrc((const uint8_t*)&h, offsetof(BlobHdr, crc));
  return ~storeCrc(data, h.size, c);
}

void storePath(char* out, const char* name, uint8_t slot) { sprintf(out, "/%s.%c", name, 'a' + slot); }

bool storeBegin() {
  if (LittleFS.begin()) return true;
  Serial.println("[store] mount failed -> formatting");
  LittleFS.format();
  return LittleFS.begin();
}

// reads one slot into storeTmp; returns true if magic + size + CRC are good
bool storeReadSlot(const char* name, uint8_t slot, BlobHdr& h, bool& existed) {
  char p[24]; storePath(p, name, slot);
  existed = LittleFS.exists(p);
  if (!existed) return false;
  File f = LittleFS.open(p, "r");
  if (!f) return false;
  bool ok = f.read((uint8_t*)&h, sizeof(h)) == sizeof(h)
         && h.magic == STORE_MAGIC && h.size <= STORE_MAX
         && f.read(storeTmp, h.size) == h.size
         && storeHdrCrc(h, storeTmp) == h.crc;
  f.close();
  return ok;
}

bool storeLoad(Blob& b) {
  b.badSlots = 0; b.slot = -1; b.seq = 0;
  BlobHdr h, best; int8_t bestSlot = -1; bool existed;
  for (uint8_t s = 0; s < 2; s++) {
    if (storeReadSlot(b.name, s, h, existed)) {
      if (bestSlot < 0 || h.seq > best.seq) { best = h; bestSlot = s; }
    } else if (existed) b.badSlots++;
  }
  if (bestSlot < 0) return false;
  storeReadSlot(b.name, bestSlot, h, existed);            // re-read the winner into storeTmp
  memcpy(b.data, storeTmp, min(b.size, best.size));       // smaller old save -> new fields keep defaults
  b.seq = best.seq; b.slot = bestSlot; b.loadedVer = best.ver;
  return true;
}

bool storeSave(Blob& b) {
  uint32_t t0 = micros();
  uint8_t slot = (b.slot == 0) ? 1 : 0;                   // always overwrite the older slot
  BlobHdr h = { STORE_MAGIC, b.ver, b.size, b.seq + 1, 0 };
  h.crc = storeHdrCrc(h, (const uint8_t*)b.data);
  char p[24]; storePath(p, b.name, slot);
  File f = LittleFS.open(p, "w");
  if (!f) return false;
  bool ok = f.write((const uint8_t*)&h, sizeof(h)) == sizeof(h)
         && f.write((const uint8_t*)b.data, b.size) == b.size;
  f.close();
  if (ok) { b.seq = h.seq; b.slot = slot; }
  b.lastSaveUs = micros() - t0;
  return ok;
}

void storeRemove(Blob& b) {
  char p[24];
  for (uint8_t s = 0; s < 2; s++) { storePath(p, b.name, s); LittleFS.remove(p); }
  b.seq = 0; b.slot = -1;
}

void storeInfo(Blob& b) {
  Serial.printf("  %-9s v%u  %3u B  seq %-6lu slot %c  bad %u  save %lu us\n",
                b.name, b.ver, b.size, (unsigned long)b.seq, b.slot < 0 ? '-' : 'a' + b.slot,
                b.badSlots, (unsigned long)b.lastSaveUs);
}
