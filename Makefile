all:server upload
server: httpserver.cpp httpserver.hpp utils.hpp 
	g++ -g -o $@ $^ -std=c++11 -pthread
upload:utils.hpp upload.cpp 
	g++ -g -o $@ $^ -std=c++11 -pthread 
.PHONY:clean
clean:
	rm server upload
