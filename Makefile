CPP = g++
CPPFLAGS = -O0 -g -I./include -std=c++11
LDFLAGS = 

CPPFILES := $(wildcard src/*.cpp)
OBJFILES := $(addprefix obj/,$(notdir $(CPPFILES:.cpp=.o)))

all: sedq

sedq: $(OBJFILES)
	$(CPP) $(LDFLAGS) -o $@ $^

obj/%.o: src/%.cpp
	$(CPP) $(CPPFLAGS) -c -o $@ $<

clean:
	rm -rf sedq obj/*.o

