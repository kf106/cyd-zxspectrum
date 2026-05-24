#pragma once

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
    void loadTape(std::string filename);
};