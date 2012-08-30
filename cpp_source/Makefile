all: libpowertutor_sim.so

#libpowertutor.so: libpowertutor.cpp timeops.cpp
#	g++ -o $@ $^ -g -O0 -Wall -Werror -shared -DBUILDING_SHLIB -pthread

MOCKTIME_LDFLAGS :=  -lmocktime

libpowertutor_sim.so: libpowertutor.cpp timeops.cpp power_model.cpp
	g++ -o $@ $^ -g -O0 -Wall -Werror -shared -DBUILDING_SHLIB -DSIMULATION_BUILD -pthread $(MOCKTIME_LDFLAGS)

install:
	-install libpowertutor_sim.so /usr/local/lib
	-install libpowertutor.h /usr/local/include

clean:
	rm -f libpowertutor.so
