# -*- Mode: Python -*-

# Add flag to env[key] if the compiler is able to build an object file
# with this. extension can be '.c' or '.cc'.
def AddOptionalFlag(context, extension, key, flag):
    context.Message('Check if compiler supports "%s"... ' % flag)
    old_var = context.env[key];
    context.env[key] =  context.env[key] + [flag]
    result = context.TryCompile('', extension)
    context.Result(result)
    if not result:
        context.env[key] = old_var
    return result

host_env = Environment(CCFLAGS = ['-O3', '-g'],
                       CXXFLAGS = [],
                       LINKFLAGS = ['-g'],
                       CPPPATH = ['#include'])

if 'host_cxx' in ARGUMENTS:
    print("Forcing host C++ compiler to %s." % ARGUMENTS['host_cxx'])
    host_env['CXX'] = ARGUMENTS['host_cxx']

if int(ARGUMENTS.get('force32', 0)):
    host_env.Append(CCFLAGS = ['-m32'],
                    LINKFLAGS = ['-m32'])

conf = Configure(host_env, custom_tests = { 'AddOptionalFlag' : AddOptionalFlag })

if not conf.AddOptionalFlag('.cc', 'CXXFLAGS', '-std=c++11') and not conf.AddOptionalFlag('.cc', 'CXXFLAGS', '-std=c++0x'):
    print("Your compiler is too old.")
    Exit(1)

conf.AddOptionalFlag('.c', 'CCFLAGS', '-march=native')

host_env = conf.Finish()

host_env.Program('test/checksums', ['test/checksums.cc'])
host_env.AddPostAction('test/checksums', Command('$SOURCE', [], 'test/checksums'))

# EOF
