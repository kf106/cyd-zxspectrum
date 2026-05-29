#include "CydKeyboardTheme.h"
#include "../Files/Files.h"
#include <cctype>
#include <cstring>
#include <cstdio>
#include <string>
#include <errno.h>

static constexpr uint32_t CYD_PACK_MAGIC = 0x544b5943; // "CYKT" little-endian
static constexpr uint32_t CYD_PACK_HEADER_SIZE = 8;
static constexpr int CYD_MAX_PACK_ENTRIES = 64;
static constexpr size_t CYD_MAX_NAME_LEN = 31;

static IFiles *s_files = nullptr;
static bool s_gamePackLoaded = false;
static bool s_useCustom = false;
static bool s_playfieldLatched = false;

static uint16_t s_scratch[32 * 32];

static FILE *s_kbdFile = nullptr;
static size_t s_kbdFileSize = 0;

struct PackEntry
{
  char name[32];
  uint16_t width;
  uint16_t height;
  uint32_t payloadOffset;
  uint32_t fileOffset;
};

static PackEntry s_packEntries[CYD_MAX_PACK_ENTRIES];
static int s_packEntryCount = 0;

static void clearGamePack()
{
  if (s_kbdFile != nullptr)
  {
    fclose(s_kbdFile);
    s_kbdFile = nullptr;
  }
  s_kbdFileSize = 0;
  s_gamePackLoaded = false;
  s_useCustom = false;
  s_packEntryCount = 0;
}

static bool isSdTapePath(const char *path)
{
  return path != nullptr && strstr(path, "/sdcard") != nullptr;
}

static bool tapePathToKbdPath(const char *tapePath, char *out, size_t outLen)
{
  if (tapePath == nullptr || outLen < 8)
  {
    return false;
  }
  const char *dot = strrchr(tapePath, '.');
  if (dot == nullptr)
  {
    snprintf(out, outLen, "%s.kbd", tapePath);
    return true;
  }
  std::string ext = dot;
  for (char &c : ext)
  {
    c = (char)tolower((unsigned char)c);
  }
  if (ext != ".tap" && ext != ".tzx")
  {
    return false;
  }
  const size_t baseLen = (size_t)(dot - tapePath);
  if (baseLen + 5 >= outLen)
  {
    return false;
  }
  memcpy(out, tapePath, baseLen);
  memcpy(out + baseLen, ".kbd", 5);
  return true;
}

static bool labelToAssetPrefix(const char *label, size_t keyIndex, char *out, size_t outLen)
{
  if (label == nullptr || label[0] == '\0' || outLen < 8)
  {
    return false;
  }
  if (strcmp(label, "Ent") == 0)
  {
    strncpy(out, "enter", outLen);
    return true;
  }
  if (strcmp(label, "Del") == 0)
  {
    strncpy(out, "del", outLen);
    return true;
  }
  if (strcmp(label, "Spc") == 0)
  {
    strncpy(out, keyIndex == 8 ? "space_left" : "space_right", outLen);
    return true;
  }
  if (strcmp(label, "Sym") == 0)
  {
    strncpy(out, "sym", outLen);
    return true;
  }
  if (strcmp(label, "Caps") == 0)
  {
    strncpy(out, "caps", outLen);
    return true;
  }
  if (strcmp(label, "Menu") == 0)
  {
    strncpy(out, "menu", outLen);
    return true;
  }
  if (strcmp(label, "R1") == 0)
  {
    strncpy(out, "r1", outLen);
    return true;
  }
  if (strcmp(label, "R2") == 0)
  {
    strncpy(out, "r2", outLen);
    return true;
  }
  if (strcmp(label, "R3") == 0)
  {
    strncpy(out, "r3", outLen);
    return true;
  }
  if (strcmp(label, "R4") == 0)
  {
    strncpy(out, "r4", outLen);
    return true;
  }
  if (label[1] == '\0')
  {
    const char c = label[0];
    if (c >= '0' && c <= '9')
    {
      snprintf(out, outLen, "key_%c", c);
      return true;
    }
    if (c >= 'A' && c <= 'Z')
    {
      out[0] = (char)(c - 'A' + 'a');
      out[1] = '\0';
      return true;
    }
    if (c >= 'a' && c <= 'z')
    {
      out[0] = c;
      out[1] = '\0';
      return true;
    }
  }
  return false;
}

static bool readU8(FILE *fp, uint8_t &out)
{
  return fread(&out, 1, 1, fp) == 1;
}

static bool readU16(FILE *fp, uint16_t &out)
{
  uint8_t b[2];
  if (fread(b, 1, 2, fp) != 2)
  {
    return false;
  }
  out = (uint16_t)b[0] | ((uint16_t)b[1] << 8);
  return true;
}

static bool readU32(FILE *fp, uint32_t &out)
{
  uint8_t b[4];
  if (fread(b, 1, 4, fp) != 4)
  {
    return false;
  }
  out = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
  return true;
}

// Pack offsets in .kbd are payload-relative (0 = first pixel byte after the index table).
static bool parseGamePackFile(FILE *fp, size_t fileSize)
{
  if (fileSize < CYD_PACK_HEADER_SIZE)
  {
    return false;
  }
  if (fseek(fp, 0, SEEK_SET) != 0)
  {
    return false;
  }

  uint8_t header[CYD_PACK_HEADER_SIZE];
  if (fread(header, 1, CYD_PACK_HEADER_SIZE, fp) != CYD_PACK_HEADER_SIZE)
  {
    return false;
  }

  uint32_t magic = (uint32_t)header[0] | ((uint32_t)header[1] << 8) | ((uint32_t)header[2] << 16) |
                   ((uint32_t)header[3] << 24);
  if (magic != CYD_PACK_MAGIC)
  {
    return false;
  }
  const uint16_t version = (uint16_t)header[4] | ((uint16_t)header[5] << 8);
  if (version != 1)
  {
    return false;
  }
  const uint16_t count = (uint16_t)header[6] | ((uint16_t)header[7] << 8);
  if (count == 0 || count > CYD_MAX_PACK_ENTRIES)
  {
    return false;
  }

  s_packEntryCount = 0;
  for (uint16_t i = 0; i < count; i++)
  {
    uint8_t nameLen = 0;
    if (!readU8(fp, nameLen))
    {
      return false;
    }
    if (nameLen == 0 || nameLen > CYD_MAX_NAME_LEN)
    {
      return false;
    }
    PackEntry &e = s_packEntries[s_packEntryCount++];
    memset(e.name, 0, sizeof(e.name));
    if (fread(e.name, 1, nameLen, fp) != nameLen)
    {
      return false;
    }
    if (!readU16(fp, e.width) || !readU16(fp, e.height))
    {
      return false;
    }
    if (!readU32(fp, e.payloadOffset))
    {
      return false;
    }
    if (e.payloadOffset < CYD_PACK_HEADER_SIZE)
    {
      return false;
    }

    const size_t pixelBytes = (size_t)e.width * e.height * sizeof(uint16_t);
    if (e.width == 0 || e.height == 0 || pixelBytes > sizeof(s_scratch))
    {
      return false;
    }
  }

  const size_t payloadBase = ftell(fp);
  for (int i = 0; i < s_packEntryCount; i++)
  {
    PackEntry &e = s_packEntries[i];
    e.fileOffset =
        (uint32_t)(payloadBase + (e.payloadOffset - CYD_PACK_HEADER_SIZE));
    const size_t pixelBytes = (size_t)e.width * e.height * sizeof(uint16_t);
    if ((size_t)e.fileOffset + pixelBytes > fileSize)
    {
      return false;
    }
  }
  return true;
}

void cydKeyboardThemeInit(IFiles *files)
{
  s_files = files;
  (void)s_files;
  clearGamePack();
}

void cydKeyboardThemeResetToBuiltin()
{
  clearGamePack();
}

bool cydKeyboardThemeHasGamePack()
{
  return s_gamePackLoaded;
}

bool cydKeyboardThemeUseCustom()
{
  return s_gamePackLoaded && s_useCustom;
}

bool cydKeyboardThemeTryLoadForTape(const char *tapePath)
{
  clearGamePack();
  if (!isSdTapePath(tapePath))
  {
    return false;
  }
  char kbdPath[256];
  if (!tapePathToKbdPath(tapePath, kbdPath, sizeof(kbdPath)))
  {
    return false;
  }

  FILE *fp = fopen(kbdPath, "rb");
  if (fp == nullptr)
  {
    Serial.printf("No keyboard pack at %s (open failed: %d)\n", kbdPath, errno);
    return false;
  }
  if (fseek(fp, 0, SEEK_END) != 0)
  {
    Serial.printf("No keyboard pack at %s (seek failed)\n", kbdPath);
    fclose(fp);
    return false;
  }
  const long fileSizeLong = ftell(fp);
  if (fileSizeLong <= 0)
  {
    Serial.printf("No keyboard pack at %s (empty file)\n", kbdPath);
    fclose(fp);
    return false;
  }
  const size_t fileSize = (size_t)fileSizeLong;

  if (!parseGamePackFile(fp, fileSize))
  {
    Serial.printf("Keyboard pack invalid: %s\n", kbdPath);
    fclose(fp);
    return false;
  }

  s_kbdFile = fp;
  s_kbdFileSize = fileSize;
  s_gamePackLoaded = true;
  s_useCustom = true;
  Serial.printf("Keyboard pack opened: %s (%d keys, %u bytes)\n", kbdPath, s_packEntryCount,
                (unsigned)fileSize);
  return true;
}

bool cydKeyboardThemeOnPlayfieldTap()
{
  if (!s_gamePackLoaded)
  {
    return false;
  }
  if (s_playfieldLatched)
  {
    return false;
  }
  s_playfieldLatched = true;
  s_useCustom = !s_useCustom;
  Serial.printf("Keyboard images: %s\n", s_useCustom ? "game pack" : "built-in");
  return true;
}

void cydKeyboardThemeClearPlayfieldLatch()
{
  s_playfieldLatched = false;
}

bool cydKeyboardThemeLoadImage(const char *label, size_t keyIndex, CydKeyImage &out)
{
  if (!cydKeyboardThemeUseCustom() || s_kbdFile == nullptr)
  {
    return false;
  }
  char prefix[24];
  if (!labelToAssetPrefix(label, keyIndex, prefix, sizeof(prefix)))
  {
    return false;
  }
  for (int i = 0; i < s_packEntryCount; i++)
  {
    if (strcmp(s_packEntries[i].name, prefix) != 0)
    {
      continue;
    }
    const PackEntry &e = s_packEntries[i];
    const size_t pixelCount = (size_t)e.width * e.height;
    const size_t pixelBytes = pixelCount * sizeof(uint16_t);
    if (pixelBytes > sizeof(s_scratch) || (size_t)e.fileOffset + pixelBytes > s_kbdFileSize)
    {
      return false;
    }
    if (fseek(s_kbdFile, (long)e.fileOffset, SEEK_SET) != 0)
    {
      return false;
    }
    if (fread(s_scratch, 1, pixelBytes, s_kbdFile) != pixelBytes)
    {
      return false;
    }
    out.width = e.width;
    out.height = e.height;
    out.data = s_scratch;
    return true;
  }
  return false;
}
