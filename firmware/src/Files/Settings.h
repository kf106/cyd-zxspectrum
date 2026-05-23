#include "./Files.h"
#include <ArduinoJson.h>
#include <cstdio>
#include <sstream>
#include <string>
#include "ISettings.h"

class Settings: public ISettings {
  private:
    bool save() {
      FILE *file = m_files->open("settings.json", "w");
      if (file) {
        std::stringstream ss;
        serializeJson(doc, ss);
        std::string jsonStr = ss.str();
        Serial.printf("Saving settings: %s\n", jsonStr.c_str());
        fwrite(jsonStr.c_str(), 1, jsonStr.size(), file);
        fclose(file);
        return true;
      } else {
        Serial.println("Failed to open settings file");
      }
      return false;
    }

    void ensureCydDefaults() {
      if (!doc["cydSetupComplete"].is<bool>()) {
        doc["cydSetupComplete"] = false;
      }
      if (!doc["cydRightHanded"].is<bool>()) {
        doc["cydRightHanded"] = false;
      }
      if (!doc["cydTouchValid"].is<bool>()) {
        doc["cydTouchValid"] = false;
      }
      if (!doc["cydTouchRawXMin"].is<uint16_t>()) {
        doc["cydTouchRawXMin"] = 200;
      }
      if (!doc["cydTouchRawXMax"].is<uint16_t>()) {
        doc["cydTouchRawXMax"] = 3700;
      }
      if (!doc["cydTouchRawYMin"].is<uint16_t>()) {
        doc["cydTouchRawYMin"] = 200;
      }
      if (!doc["cydTouchRawYMax"].is<uint16_t>()) {
        doc["cydTouchRawYMax"] = 3700;
      }
      if (!doc["cydTouchSwapXY"].is<bool>()) {
        doc["cydTouchSwapXY"] = true;
      }
    }

  public:
    Settings(IFiles *files) : m_files(files) {
      bool loaded = false;
      FILE *file = m_files->open("settings.json", "r");
      if (file) {
        fseek(file, 0, SEEK_END);
        long fileSize = ftell(file);
        fseek(file, 0, SEEK_SET);
        if (fileSize > 0) {
          std::string jsonStr(fileSize, '\0');
          fread(&jsonStr[0], 1, fileSize, file);
          std::stringstream ss(jsonStr);
          DeserializationError error = deserializeJson(doc, ss);
          loaded = error.code() == DeserializationError::Ok;
        }
        fclose(file);
      }
      if (!loaded) {
        Serial.println("No settings found, creating default");
        doc["volume"] = 10;
      }
      ensureCydDefaults();
      if (!loaded) {
        save();
      }
    }

    int getVolume() override {
      return doc["volume"];
    }
    void setVolume(int volume) override {
      doc["volume"] = volume;
      save();
    }

    bool isCydSetupComplete() override {
      return doc["cydSetupComplete"].as<bool>();
    }
    void setCydSetupComplete(bool complete) override {
      doc["cydSetupComplete"] = complete;
      save();
    }

    bool isCydRightHanded() override {
      return doc["cydRightHanded"].as<bool>();
    }
    void setCydRightHanded(bool rightHanded) override {
      doc["cydRightHanded"] = rightHanded;
      save();
    }

    CydTouchCalibration getCydTouchCalibration() override {
      CydTouchCalibration cal;
      cal.rawXMin = doc["cydTouchRawXMin"].as<uint16_t>();
      cal.rawXMax = doc["cydTouchRawXMax"].as<uint16_t>();
      cal.rawYMin = doc["cydTouchRawYMin"].as<uint16_t>();
      cal.rawYMax = doc["cydTouchRawYMax"].as<uint16_t>();
      cal.swapXY = doc["cydTouchSwapXY"].as<bool>();
      cal.valid = doc["cydTouchValid"].as<bool>();
      return cal;
    }

    void setCydTouchCalibration(const CydTouchCalibration &cal) override {
      doc["cydTouchRawXMin"] = cal.rawXMin;
      doc["cydTouchRawXMax"] = cal.rawXMax;
      doc["cydTouchRawYMin"] = cal.rawYMin;
      doc["cydTouchRawYMax"] = cal.rawYMax;
      doc["cydTouchSwapXY"] = cal.swapXY;
      doc["cydTouchValid"] = cal.valid;
      save();
    }

  private:
    IFiles *m_files;
    JsonDocument doc;
};
