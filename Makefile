
Bridge: Bridge.o
	g++ -std=c++17 Bridge.o -pthread -L/usr/lib/x86_64-linux-gnu/ -lboost_system -lboost_thread -lboost_random -lboost_filesystem -lboost_context -lboost_program_options -lcryptopp -L/usr/local/lib/ -lscrypt -o Bridge
	rm *.o

Bridge.o: Bridge.cpp
	g++ -O0 -DDEBUG -g -std=c++11 -I . -c Bridge.cpp -fpermissive -DHAVE_CONFIG_H 
	#-Ofast

clean:
	rm *.o
