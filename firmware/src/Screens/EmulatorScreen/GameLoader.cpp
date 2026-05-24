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
    Serial.printf("Failed to reserve tape buffer (largest %u, free %u, need %u)\n",
                  (unsigned)largest, ESP.getFreeHeap(), (unsigned)CYD_TAPE_BUFFER_MIN);
    return;
  }

  while (size >= CYD_TAPE_BUFFER_MIN)
  {
    s_tapeBuffer = (uint8_t *)heap_caps_malloc(size, MALLOC_CAP_8BIT);
    if (s_tapeBuffer != nullptr)
    {
      s_tapeBufferSize = size;
      Serial.printf("Reserved tape buffer: %u bytes (largest block was %u)\n",
                    (unsigned)size, (unsigned)largest);
      return;
    }
    size -= 4096;
  }

  Serial.printf("Failed to reserve tape buffer (largest %u, free %u)\n",
                (unsigned)largest, ESP.getFreeHeap());
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

void GameLoader::loadTape(std::string filename)
{
  ScopeGuard guard([&]()
                   {
    renderer->setIsLoading(false);
    if (audioOutput) audioOutput->resume(); });
  // stop audio playback
  if (audioOutput)
  {
    audioOutput->pause();
  }
  uint64_t startTime = get_usecs();
  renderer->setIsLoading(true);
  Serial.printf("Loading tape %s\n", filename.c_str());
  Serial.printf("Loading tape file\n");
  FILE *fp = fopen(filename.c_str(), "rb");
  if (fp == NULL)
  {
    Serial.println("Error: Could not open file.");
    std::cout << "Error: Could not open file." << std::endl;
    return;
  }
  fseek(fp, 0, SEEK_END);
  long file_size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  Serial.printf("File size %ld\n", file_size);
  if (s_tapeBuffer == nullptr)
  {
    Serial.println("Error: Tape buffer was not reserved at boot.");
    fclose(fp);
    return;
  }
  if (file_size > (long)s_tapeBufferSize)
  {
    Serial.printf("Error: Tape file too large (%ld bytes, max %u).\n", file_size, (unsigned)s_tapeBufferSize);
    fclose(fp);
    return;
  }
  bool ownsTapeBuffer = false;
  uint8_t *tzx_data = allocateTapeBuffer(file_size, ownsTapeBuffer);
  if (!tzx_data)
  {
    Serial.printf("Error: Could not allocate memory for tape (%ld bytes, free heap %u, largest block %u).\n",
                  file_size, ESP.getFreeHeap(),
                  heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    fclose(fp);
    return;
  }
  if (fread(tzx_data, 1, file_size, fp) != (size_t)file_size)
  {
    Serial.println("Error: Failed to read tape file.");
    if (ownsTapeBuffer)
    {
      free(tzx_data);
    }
    fclose(fp);
    return;
  }
  fclose(fp);
  // load the tape
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
  int count = 0;
  ZXSpectrumTapeListener *listener = new ZXSpectrumTapeListener(machine->getMachine(), [&](uint64_t progress)
                                                                {
        count++;
        if (count % 4000 == 0) {
          float machineTime = (float) listener->getTotalTicks() / 3500000.0f;
          float wallTime = (float) (get_usecs() - startTime) / 1000000.0f;
          Serial.printf("Total execution time: %fs\n", (float) listener->getTotalExecutionTime() / 1000000.0f);
          Serial.printf("Total machine time: %f\n", machineTime);
          Serial.printf("Wall Clock time: %fs\n", wallTime);
          Serial.printf("Speed Up: %f\n",  machineTime/wallTime);
          Serial.printf("Progress: %lld\n", progress * 100 / totalTicks);
          renderer->setLoadProgress(progress * 100 / totalTicks);
          renderer->forceRedraw(machine->getMachine()->mem.currentScreen->data,
                                machine->getMachine()->borderColors);
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
}