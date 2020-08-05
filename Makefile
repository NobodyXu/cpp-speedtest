CXX := clang++
LDFLAGS := -lpthread -Wl,--plugin-opt=O3,-O2,--discard-all,--strip-all,--gc-sections

include curl-cpp/linking.mk

CXXFLAGS += -flto -O3 -fno-exceptions -fno-rtti -fno-asynchronous-unwind-tables -fno-unwind-tables 

SRCS=$(shell find */* -type f -name '*.cc' -not -path ./test -not -path ./curl-cpp/)
DEPS=$(SRCS:.cc=.d)
OBJS=$(SRCS:.cc=.o)

TARGET_BIN=cpp-speedtest

# Autobuild dependency, adapted from:
#    http://make.mad-scientist.net/papers/advanced-auto-dependency-generation/#include
DEPFLAGS = -MT $@ -MMD -MP -MF $*.Td

$(DEPS) cpp-speedtest.d: 
include $(wildcard $(DEPS))

## Disable implict pattern
%.o : %.cc
%.o : %.cc %.d
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(DEPFLAGS)
	mv -f $*.Td $*.d && touch $@

clean:
	rm -rf *.o $(DEPS) $(DEPS:.d=.Td) $(OBJS)

.PHONY: clean
