#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

class Machine;
class AudioOutput;
class Renderer;

class GameLoader
{
  private:
    Machine *machine = nullptr;
    Renderer *renderer = nullptr;
    AudioOutput *audioOutput = nullptr;
  public:
    GameLoader(Machine *machine, Renderer *renderer, AudioOutput *audioOutput);
    static void reserveTapeBuffer(size_t size);
    static bool ensureWorkspaceBuffer();
    static uint8_t *workspaceBuffer();
    static size_t workspaceCapacity();
    static bool readFileIntoWorkspace(const char *path, size_t &outSize);
    static bool probeTapeFile(const char *path, long &outFileSize);
    bool loadTape(std::string filename);
};
