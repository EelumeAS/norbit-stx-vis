
ifeq ($(PREFIX),)
	PREFIX=/usr/local
endif

run: main.cpp
	g++ -lGL -lGLEW -lSDL2 main.cpp -o run

clean:
	rm run
