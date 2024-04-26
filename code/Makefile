debug:
	mpic++ -o main helpers.cpp cnf.cpp interconnect.cpp state.cpp main.cpp -pg -g

fast:
	mpic++ -o main helpers.cpp cnf.cpp interconnect.cpp state.cpp main.cpp -DNDEBUG

fast_profiled:
	mpic++ -o main helpers.cpp cnf.cpp interconnect.cpp state.cpp main.cpp -pg -DNDEBUG -g

clean:
	/bin/rm -rf *~ *.o $(APP_NAME) *.class