CPP = g++
CPPFLAGS = -O0 -g -I./include -std=c++11 -D_TRACE
LDFLAGS = 

CPPFILES := $(wildcard src/*.cpp)
OBJFILES := $(addprefix obj/,$(notdir $(CPPFILES:.cpp=.o)))

all: sedq

sedq: $(OBJFILES) obj/main.o
	$(CPP) $(LDFLAGS) -o $@ $^

obj/main.o: frontend/main.cpp
	$(CPP) $(CPPFLAGS) -c frontend/main.cpp -o obj/main.o 

obj/%.o: src/%.cpp
	$(CPP) $(CPPFLAGS) -c -o $@ $<

clean:
	rm -rf sedq obj/*.o

