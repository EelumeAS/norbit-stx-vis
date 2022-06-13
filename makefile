
ifeq ($(PREFIX),)
	PREFIX=/usr/local
endif

run: main.cpp TcpClient.cpp
	g++ -lGL -lGLEW -lSDL2 `pkg-config --cflags --libs opencv4` main.cpp TcpClient.cpp -o run

clean:
	rm run
