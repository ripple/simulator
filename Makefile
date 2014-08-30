# Copyright (c) 2009-2010 Satoshi Nakamoto
# Distributed under the MIT/X11 software license, see the accompanying
# file license.txt or http://www.opensource.org/licenses/mit-license.php.

DEFS       =
LIBS       = -pthread
DEBUGFLAGS = -DDEBUG -g
CXXFLAGS   = -O0 -std=c++11 -Wall -Wno-sign-compare -Wno-char-subscripts \
             -Wno-invalid-offsetof -Wformat $(DEBUGFLAGS) $(DEFS)
HEADERS    = Core.h

SRCS       = Sim.cpp Util.cpp

OBJS       = $(SRCS:%.cpp=%.o)

all: sim

%.o:	%.cpp
	$(CXX) -c $(CXXFLAGS) -o $@ $<


sim:	$(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

.dep:
	$(CXX) -M $(SRCS) $(CXXFLAGS) > .dep

clean:
	-rm -f sim *.o

include .dep
