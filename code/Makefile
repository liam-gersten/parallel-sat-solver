debug:
	mpic++ -o main helpers.cpp cnf.cpp interconnect.cpp state.cpp main.cpp -g

fast:
	mpic++ -O3 -std=c++17 -o main helpers.cpp cnf.cpp interconnect.cpp state.cpp main.cpp -DNDEBUG

fast_profiled:
	mpic++ -o2 -o main helpers.cpp cnf.cpp interconnect.cpp state.cpp main.cpp -DNDEBUG -g

clean:
	/bin/rm -rf *~ *.o $(APP_NAME) *.class