//
// Created by psy on 7/10/22.
//

#include <udpbuffer.hpp>
#include <multicastsync.hpp>
#include <FastLED.h>
#include "utils.h"
#include <WiFiUdp.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Arduino.h>
#include "config.h"
#include "util.hpp"

// dutycycle function to make stuff blink without using memory
// returns true during 'on' ms, with total periode time 'total' ms.
bool
duty_cycle(unsigned long on, unsigned long total, unsigned long starttime = 0) {
    if (!starttime)
        return ((multicastSync.remoteMillis() % total) < on);
    else
        return (((multicastSync.remoteMillis() - starttime) % total) < on);
}