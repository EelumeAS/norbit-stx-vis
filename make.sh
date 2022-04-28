#!/bin/bash
g++ -lGL -lGLEW -lSDL2 `pkg-config --cflags --libs opencv4` main.cpp
