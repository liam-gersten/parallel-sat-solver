debug:
	mpic++ -o main helpers.cpp cnf.cpp interconnect.cpp state.cpp main.cpp -pg

fast:
	mpic++ -O3 -o main helpers.cpp cnf.cpp interconnect.cpp state.cpp main.cpp -DNDEBUG

fast_profiled:
	mpic++ -o main helpers.cpp cnf.cpp interconnect.cpp state.cpp main.cpp -pg -DNDEBUG

clean:
	/bin/rm -rf *~ *.o $(APP_NAME) *.class