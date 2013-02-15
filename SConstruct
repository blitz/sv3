# -*- Mode: Python -*-

host_env = Environment(#CXX = "clang++",
                       CXXFLAGS = ['-std=c++11', '-O3', '-march=corei7', '-g'],
                       LINKFLAGS = ['-g'],
                       CPPPATH = ['#include'])


host_env.Program('test/checksums', ['test/checksums.cc'])

# EOF
