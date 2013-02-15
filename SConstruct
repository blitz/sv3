# -*- Mode: Python -*-

host_env = Environment(#CXX = "clang++",
                       CXXFLAGS = ['-std=c++11', '-O3', '-march=corei7', '-g'],
                       LINKFLAGS = ['-g'],
                       CPPPATH = ['#include'])

if 'host_cxx' in ARGUMENTS:
    print("Forcing host C++ compiler to %s." % ARGUMENTS['host_cxx'])
    host_env['CXX'] = ARGUMENTS['host_cxx']


host_env.Program('test/checksums', ['test/checksums.cc'])
host_env.AddPostAction('test/checksums', Command('$SOURCE', [], 'test/checksums'))

# EOF
