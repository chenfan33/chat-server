TARGETS = chatserver chatclient

all: $(TARGETS)

%.o: %.cc
	g++ $^ -c -o $@
	g++ Multicast.cc -c -o Multicast.o

chatserver: chatserver.o
	g++ $^ Multicast.o -o $@

chatclient: chatclient.o
	g++ $^ -o $@



pack:
	rm -f submit-hw3.zip
	zip -r submit-hw3.zip README Makefile *.c* *.h*

clean::
	rm -fv $(TARGETS) *~ *.o submit-hw3.zip

realclean:: clean
	rm -fv submit-hw3.zip
