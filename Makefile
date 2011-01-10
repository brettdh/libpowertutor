all: libpowertutor.so

libpowertutor.so: libpowertutor.cpp timeops.cpp
	g++ -o $@ $^ -g -O0 -Wall -Werror -shared -DBUILDING_SHLIB -pthread

clean:
	rm -f libpowertutor.so
