#include <iostream>
#include <Arduino.h>
#include <esp_heap_caps.h>
#include "../../TZX/ZXSpectrumTapeListener.h"
#include "../../TZX/DummyListener.h"
#include "../../TZX/tzx_cas.h"
#include "./Machine.h"
#include "./GameLoader.h"
#include "./Renderer.h"
#include "../../AudioOutput/AudioOutput.h"
#include "../../utils.h"

static uint8_t *s_tapeBuffer = nullptr;
static size_t s_tapeBufferSize = 0;

void GameLoader::reserveTapeBuffer(size_t maxSize)
{
  if (s_tapeBuffer != nullptr)
  {
    return;
  }

  size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  size_t size = maxSize;
  if (size > largest)
  {
    size = largest & ~0x3FF;
  }

#ifndef CYD_TAPE_BUFFER_MIN
#define CYD_TAPE_BUFFER_MIN 49152
#endif
  if (size < CYD_TAPE_BUFFER_MIN)
  {
    Serial.printf("Failed to reserve workspace (largest %u, free %u, need %u)\n",
                  (unsigned)largest, ESP.getFreeHeap(), (unsigned)CYD_TAPE_BUFFER_MIN);
    return;
  }

  while (size >= CYD_TAPE_BUFFER_MIN)
  {
    s_tapeBuffer = (uint8_t *)heap_caps_malloc(size, MALLOC_CAP_8BIT);
    if (s_tapeBuffer != nullptr)
    {
      s_tapeBufferSize = size;
      Serial.printf("Reserved workspace buffer: %u bytes (largest block was %u)\n",
                    (unsigned)size, (unsigned)largest);
      return;
    }
    size -= 4096;
  }

  Serial.printf("Failed to reserve workspace buffer (largest %u, free %u)\n",
                (unsigned)largest, ESP.getFreeHeap());
}

bool GameLoader::ensureWorkspaceBuffer()
{
  if (s_tapeBuffer != nullptr)
  {
    return true;
  }
#ifdef CYD_TAPE_BUFFER_SIZE
  reserveTapeBuffer(CYD_TAPE_BUFFER_SIZE);
#else
  reserveTapeBuffer(98304);
#endif
  return s_tapeBuffer != nullptr;
}

uint8_t *GameLoader::workspaceBuffer()
{
  return s_tapeBuffer;
}

size_t GameLoader::workspaceCapacity()
{
  return s_tapeBufferSize;
}

bool GameLoader::probeTapeFile(const char *path, long &outFileSize)
{
  outFileSize = 0;
  if (path == nullptr || !ensureWorkspaceBuffer())
  {
    return false;
  }
  FILE *fp = fopen(path, "rb");
  if (fp == nullptr)
  {
    Serial.printf("Tape file not found: %s\n", path);
    return false;
  }
  if (fseek(fp, 0, SEEK_END) != 0)
  {
    fclose(fp);
    return false;
  }
  outFileSize = ftell(fp);
  fclose(fp);
  if (outFileSize <= 0)
  {
    Serial.printf("Tape file empty: %s\n", path);
    return false;
  }
  if (outFileSize > (long)s_tapeBufferSize)
  {
    Serial.printf("Tape too large: %ld bytes (workspace %u)\n", outFileSize, (unsigned)s_tapeBufferSize);
    return false;
  }
  return true;
}

bool GameLoader::readFileIntoWorkspace(const char *path, size_t &outSize)
{
  outSize = 0;
  if (s_tapeBuffer == nullptr || path == nullptr)
  {
    return false;
  }
  FILE *fp = fopen(path, "rb");
  if (fp == nullptr)
  {
    return false;
  }
  if (fseek(fp, 0, SEEK_END) != 0)
  {
    fclose(fp);
    return false;
  }
  long fileSize = ftell(fp);
  if (fileSize <= 0 || (size_t)fileSize > s_tapeBufferSize)
  {
    fclose(fp);
    return false;
  }
  if (fseek(fp, 0, SEEK_SET) != 0)
  {
    fclose(fp);
    return false;
  }
  if (fread(s_tapeBuffer, 1, (size_t)fileSize, fp) != (size_t)fileSize)
  {
    fclose(fp);
    return false;
  }
  fclose(fp);
  outSize = (size_t)fileSize;
  return true;
}

GameLoader::GameLoader(Machine *machine, Renderer *renderer, AudioOutput *audioOutput) : machine(machine), renderer(renderer), audioOutput(audioOutput) {}

static uint8_t *allocateTapeBuffer(long file_size, bool &ownsBuffer)
{
  ownsBuffer = false;
  if (file_size <= 0)
  {
    return nullptr;
  }
  if (s_tapeBuffer != nullptr && (size_t)file_size <= s_tapeBufferSize)
  {
    return s_tapeBuffer;
  }
  uint8_t *buffer = (uint8_t *)heap_caps_malloc(file_size, MALLOC_CAP_8BIT);
  if (!buffer)
  {
    buffer = (uint8_t *)malloc(file_size);
  }
  if (buffer)
  {
    ownsBuffer = true;
  }
  return buffer;
}

bool GameLoader::loadTape(std::string filename)
{
  if (!ensureWorkspaceBuffer())
  {
    Serial.println("Error: Workspace buffer not available for tape load.");
    return false;
  }
  ScopeGuard guard([&]()
                   {
    renderer->setIsLoading(false);
    if (audioOutput) audioOutput->resume(); });
  if (audioOutput)
  {
    audioOutput->pause();
  }
  uint64_t startTime = get_usecs();
  renderer->setIsLoading(true);
  Serial.printf("Loading tape %s\n", filename.c_str());
  FILE *fp = fopen(filename.c_str(), "rb");
  if (fp == NULL)
  {
    Serial.println("Error: Could not open file.");
    return false;
  }
  fseek(fp, 0, SEEK_END);
  long file_size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  Serial.printf("File size %ld\n", file_size);
  if (s_tapeBuffer == nullptr)
  {
    Serial.println("Error: Workspace buffer was not reserved at boot.");
    fclose(fp);
    return false;
  }
  if (file_size > (long)s_tapeBufferSize)
  {
    Serial.printf("Error: Tape file too large (%ld bytes, max %u).\n", file_size, (unsigned)s_tapeBufferSize);
    fclose(fp);
    return false;
  }
  bool ownsTapeBuffer = false;
  uint8_t *tzx_data = allocateTapeBuffer(file_size, ownsTapeBuffer);
  if (!tzx_data)
  {
    Serial.printf("Error: Could not allocate memory for tape (%ld bytes, free heap %u, largest block %u).\n",
                  file_size, ESP.getFreeHeap(),
                  heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    fclose(fp);
    return false;
  }
  if (fread(tzx_data, 1, file_size, fp) != (size_t)file_size)
  {
    Serial.println("Error: Failed to read tape file.");
    if (ownsTapeBuffer)
    {
      free(tzx_data);
    }
    fclose(fp);
    return false;
  }
  fclose(fp);
  TzxCas tzxCas;
  DummyListener *dummyListener = new DummyListener();
  dummyListener->start();
  if (filename.find(".tap") != std::string::npos || filename.find(".TAP") != std::string::npos)
  {
    tzxCas.load_tap(dummyListener, tzx_data, file_size);
  }
  else
  {
    tzxCas.load_tzx(dummyListener, tzx_data, file_size);
  }
  dummyListener->finish();
  uint64_t totalTicks = dummyListener->getTotalTicks();
  Serial.printf("Total cycles: %lld\n", dummyListener->getTotalTicks());
  delete dummyListener;
  renderer->setLoadProgress(10);
  {
    ZXSpectrum *speccy = machine->getMachine();
    renderer->forceRedraw(speccy->mem.currentScreen->data, speccy->borderColors);
  }
  int count = 0;
  ZXSpectrumTapeListener *listener = new ZXSpectrumTapeListener(machine->getMachine(), [&](uint64_t progress)
                                                                {
        count++;
        if (count % 2000 == 0) {
          const uint16_t pct = (uint16_t)(10 + (progress * 90 / totalTicks));
          float machineTime = (float) listener->getTotalTicks() / 3500000.0f;
          float wallTime = (float) (get_usecs() - startTime) / 1000000.0f;
          Serial.printf("Total execution time: %fs\n", (float) listener->getTotalExecutionTime() / 1000000.0f);
          Serial.printf("Total machine time: %f\n", machineTime);
          Serial.printf("Wall Clock time: %fs\n", wallTime);
          Serial.printf("Speed Up: %f\n",  machineTime/wallTime);
          Serial.printf("Progress: %lld\n", progress * 100 / totalTicks);
          renderer->setLoadProgress(pct);
          ZXSpectrum *speccy = machine->getMachine();
          renderer->forceRedraw(speccy->mem.currentScreen->data, speccy->borderColors);
          vTaskDelay(1);
        } });
  listener->start();
  if (filename.find(".tap") != std::string::npos || filename.find(".TAP") != std::string::npos)
  {
    Serial.printf("Loading tap file\n");
    tzxCas.load_tap(listener, tzx_data, file_size);
  }
  else
  {
    Serial.printf("Loading tzx file\n");
    tzxCas.load_tzx(listener, tzx_data, file_size);
  }
  Serial.printf("Tape loaded\n");
  listener->finish();
  Serial.printf("*********************");
  Serial.printf("Total execution time: %lld\n", listener->getTotalExecutionTime());
  Serial.printf("Total cycles: %lld\n", listener->getTotalTicks());
  Serial.printf("*********************");
  if (ownsTapeBuffer)
  {
    free(tzx_data);
  }
  delete listener;
  return true;
}
