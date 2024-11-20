.PHONY: clean 

all: compile

compile: sched_demo_313553053.c
	gcc sched_demo_313553053.c -lpthread  -o sched_demo_313553053

test: 
	sudo ./sched_test.sh ./sched_demo ./sched_demo_313553053

clean:
	rm -f sched_demo_313553053 