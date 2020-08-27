all: test.out

test.out: test.cpp ../include/entity.hpp ../include/component.hpp ../include/chunk.hpp
	g++ -W -Wall -ansi -pedantic -std=c++17 -I../include/ -O3 test.cpp -o test.out

clean:
	rm -rf test
