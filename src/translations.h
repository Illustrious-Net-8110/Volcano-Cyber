// translations.h
#pragma once
#include <map>
#include <StreamString.h>

namespace VolcanoTranslations {
  extern String currentLang;
} // namespace VolcanoTranslations

const std::map<String, std::map<String, String>> translations = {
  { "btn_cal",      {{ "de", "Kal" },       { "en", "Cal" }} },
  { "btn_on",       {{ "de", "An" },        { "en", "On" }} },
  { "btn_off",      {{ "de", "Aus" },       { "en", "Off" }} },
  { "btn_stop",     {{ "de", "Stop" },      { "en", "Stop" }} },
  { "btn_small",    {{ "de", "Klein" },     { "en", "Small" }} },
  { "btn_half",     {{ "de", "1/2" },       { "en", "1/2" }} },
  { "btn_full",     {{ "de", "Voll" },      { "en", "Full" }} },
  { "msg_connect",  {{ "de", "Zum Einrichten mit Volcano Cyber verbinden" },
                     { "en", "Connect to Volcano Cyber to configure" }} }
};
