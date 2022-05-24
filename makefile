
ifeq ($(PREFIX),)
	PREFIX=/usr/local
endif

run: main.cpp
	g++ -lGL -lGLEW -lSDL2 `pkg-config --cflags --libs opencv4` main.cpp -o run

clean:
	rm run
