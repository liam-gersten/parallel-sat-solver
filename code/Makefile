APP_NAME=main

OBJS=helpers.o cnf.o interconnect.o main.o

CXX = g++
CXXFLAGS = -Wall -Os -std=c++17 -m64 -I. -fopenmp -Wno-unknown-pragmas -pg

all: $(APP_NAME)

$(APP_NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp %.h
	$(CXX) $(CXXFLAGS) -c $<

clean:
	/bin/rm -rf *~ *.o $(APP_NAME) *.class