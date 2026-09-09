#include <Arduino.h>

#include "app/AppController.h"

namespace {
aurageek::app::AppController application;
}

void setup() {
  application.begin();
}

void loop() {
  application.process();
}
