# Lunar-Gravity-Sim
A basic physics simulator that simulates a probe in a two body (earth/moon) system. Spawn probes and give them a vector and watch them fly!


# Build Instructions

You must have raylib installed on your system for this to work. Please download the raylib installer and run it so that your main raylib folder is at C:. You can get the installer for free here:

https://raysan5.itch.io/raylib/download/eyJpZCI6ODUzMzEsImV4cGlyZXMiOjE3NjU3NDQ3NTZ9.%2ffWRk9Zo2%2bj2Em5QITg%2b9eTeNiU%3d

## Compile
```
C:\raylib\w64devkit\bin\g++ main.cpp -o main.exe -I"C:/raylib/raylib/src" -L"C:/raylib/raylib/src" -lraylib -lopengl32 -lgdi32 -lwinmm
```

## Run
```
main.exe
```
