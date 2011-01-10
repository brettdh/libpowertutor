all: libpowertutor.so

libpowertutor.so: libpowertutor.cpp timeops.cpp
	g++ -o $@ $^ -g -O0 -Wall -Werror -shared

clean:
	rm -f libpowertutor.so
