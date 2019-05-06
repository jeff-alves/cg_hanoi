#!/usr/bin/env bash

APP="hanoi"

rm -f $APP;
g++ -o $APP main.cpp -lglut -lGLU -lGL;
$(command -v optirun) ./$APP &