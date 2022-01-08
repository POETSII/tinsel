# SPDX-License-Identifier: BSD-2-Clause
APP_CPP = NHood.cpp
APP_HDR = NHood.h
RUN_CPP = Run.cpp 
RUN_H = 

include ../util/polite.mk

# Mathew: forgive me for my lazy/hacky makefiles
build/BuildGraphOnly : $(wildcard *.cpp *.h ../../../include/*.* ../../../include/POLite/*.* )
	$(CXX) -g -O3 -I ../../../include -I ../../../hostlink  BuildGraphOnly.cpp -o $@ -lmetis -lscotch