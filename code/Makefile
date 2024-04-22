sequential:
	g++ -Wall -Os -std=c++17 -m64 -I. -fopenmp -Wno-unknown-pragmas -pg -c helpers.cpp cnf.cpp interconnect.cpp state.cpp main.cpp

debug:
	mpic++ -o main helpers.cpp cnf.cpp interconnect.cpp state.cpp main.cpp -pg

fast:
	mpic++ -o main helpers.cpp cnf.cpp interconnect.cpp state.cpp main.cpp -DNDEBUG

clean:
	/bin/rm -rf *~ *.o $(APP_NAME) *.class