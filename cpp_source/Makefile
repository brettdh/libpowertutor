all: libpowertutor.so libpowertutor_sim.so

MOCKTIME_LDFLAGS :=  -lmocktime

SRCS := libpowertutor.cpp timeops.cpp power_model.cpp
DEPS := debug.h libpowertutor.h pthread_util.h timeops.h utils.h

libpowertutor.so: $(SRCS) $(DEPS)
	g++ -o $@ $(SRCS) -g -O0 -fpic -Wall -Werror -shared -DBUILDING_SHLIB -pthread $(MOCKTIME_LDFLAGS)

libpowertutor_sim.so: $(SRCS) $(DEPS)
	g++ -o $@ $(SRCS) -g -O0 -fpic -Wall -Werror -shared -DBUILDING_SHLIB -DSIMULATION_BUILD -pthread $(MOCKTIME_LDFLAGS)

install:
	-install libpowertutor.so /usr/local/lib
	-install libpowertutor_sim.so /usr/local/lib
	-install libpowertutor.h /usr/local/include

clean:
	rm -f libpowertutor.so libpowertutor_sim.so *.o *~
