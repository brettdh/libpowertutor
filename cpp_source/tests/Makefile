#all: test_sending_estimation_server
all: test_simulation

test_sending_estimation_server: test_sending_estimation.cpp
	g++ -o $@ $^ -DSERVER_ONLY -g -O0 -Wall -Werror -I.. -L.. -pthread -lpowertutor

test_simulation: run_simulation_test.cpp
	g++ -o $@ $^ -g -O0 -Wall -Werror -I.. -L.. -lpowertutor_sim -lmocktime -lcppunit

clean:
	rm -f test_sending_estimation_server test_simulation
