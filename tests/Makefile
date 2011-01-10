all: test_sending_estimation_server

test_sending_estimation_server: test_sending_estimation.cpp
	g++ -o $@ $^ -DSERVER_ONLY -g -O0 -Wall -Werror -I.. -L.. -lpowertutor

clean:
	rm -f test_sending_estimation_server
