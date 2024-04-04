sequential:
	g++ -Wall -Os -std=c++17 -m64 -I. -fopenmp -Wno-unknown-pragmas -pg -c helpers.cpp cnf.cpp interconnect.cpp state.cpp main.cpp

parallel:
	mpic++ -o main helpers.cpp cnf.cpp interconnect.cpp state.cpp main.cpp

clean:
	/bin/rm -rf *~ *.o $(APP_NAME) *.class