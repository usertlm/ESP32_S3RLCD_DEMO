#pragma once

#include <Arduino.h>
#include <U8g2lib.h>
#include "ST7305_U8g2.h"
#include "ld2450.h"

void displayManagerBegin();
void displayManagerLoop();

U8G2* displayGetU8g2();
LD2450& displayGetRadar();