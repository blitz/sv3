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

conf.AddOptionalFlag('.c', 'CCFLAGS',   '-march=corei7')
conf.AddOptionalFlag('.c', 'LINKFLAGS', '-march=corei7')
conf.AddOptionalFlag('.c', 'CCFLAGS', '-Wall')
#conf.AddOptionalFlag('.cc', 'CXXFLAGS', '-Weffc++')

host_env = conf.Finish()

host_pcap_env = host_env.Clone()
conf = Configure(host_pcap_env)
if not conf.CheckLibWithHeader('pcap', 'pcap.h', 'c'):
    print 'Could not find libpcap.'
    pcap_is_available = False
else:
    pcap_is_available = True
host_pcap_env = conf.Finish()

## Switch library

host_env.StaticLibrary('switch', Glob('switch/*.cc'))

## Programs
ts = host_env.Program('test-switch', ['switch.cc'], LIBS = ['switch'], LIBPATH = ['.'])
# Clean leftover core files as well
Clean(ts, Glob("core.*"))

# Tests

host_env.Program('test/checksums', ['test/checksums.cc'])
Command('test/checksums.log', ['test/checksums'], '$SOURCE | tee $TARGET')

host_env.Program('test/etherhash', ['test/etherhash.cc'])
host_env.Command('test/etherhash.log', ['test/etherhash' ], '$SOURCE | tee $TARGET')

if pcap_is_available:
    host_pcap_env.Program('test/packets', ['test/packets.cc'])
    Command('test/packets-ipv4-tcp.log', ['test/packets', 'test/data/ipv4-tcp.pcap' ], '! ${SOURCES[0]} ${SOURCES[1]} | tee $TARGET | grep -q wrong')
    Command('test/packets-ipv6-tcp.log', ['test/packets', 'test/data/ipv6-tcp.pcap' ], '! ${SOURCES[0]} ${SOURCES[1]} | tee $TARGET | grep -q wrong')
else:
    print("Not building test/packets! Not running tests!")

# EOF
