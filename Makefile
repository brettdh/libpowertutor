all: libpowertutor.so

libpowertutor.so: libpowertutor.cpp
	g++ -o $@ $^ -g -O0 -Wall -Werror -shared
