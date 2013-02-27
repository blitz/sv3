# -*- Mode: Python -*-

print("Use 'scons -h' to show build help.")

Help("""
Usage: scons [force32=0/1] [cxx=COMPILER] [cpu=CPU]

force32=0/1   Force 32-bit build, if force32=1. Default is 0.
debug=0/1     Build a debug version, if debug=1. Default is 0.
cxx=COMPILER  Force build to use a specific C++ compiler
cpu=CPU       Optimize for the given CPU. Passed to -march=
""")



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

preproc_test_source_file = """
#include <%s>
int main(int argc, char **argv)
{
#ifdef %s
  return 0;
#else
#error No
#endif
}"""

def CheckPreprocessorMacro(context, header, macro):
    context.Message("Looking for %s in %s..." % (macro, header))
    result = context.TryLink(preproc_test_source_file % (header, macro), '.c')
    context.Result(result)
    return result

host_env = Environment(CCFLAGS = ['-pthread'],
                       CXXFLAGS = [],
                       LINKFLAGS = ['-pthread'],
                       CPPPATH = ['#include'])

if 'cxx' in ARGUMENTS:
    print("Forcing host C++ compiler to %s." % ARGUMENTS['cxx'])
    host_env['CXX'] = ARGUMENTS['cxx']

if int(ARGUMENTS.get('debug', 1)):
    host_env.Append(CCFLAGS   = ['-O1', '-g'],
                    LINKFLAGS = ['-g'])
else:
    host_env.Append(CCFLAGS   = ['-O3'])

if int(ARGUMENTS.get('force32', 0)):
    host_env.Append(CCFLAGS = ['-m32'],
                    LINKFLAGS = ['-m32'])

conf = Configure(host_env, custom_tests = { 'AddOptionalFlag' : AddOptionalFlag ,
                                            'CheckPreprocessorMacro' : CheckPreprocessorMacro,
                                            })

if not conf.CheckPreprocessorMacro('linux/if_tun.h', 'TUNGETVNETHDRSZ'):
    conf.env.Append(CCFLAGS = ['-DNO_GETVNETHDRSZ'])

if not conf.CheckType('struct virtio_net_hdr', '#include <linux/virtio_net.h>\n'):
    print("Your Linux headers are too old.")
    Exit(1)

if not conf.AddOptionalFlag('.cc', 'CXXFLAGS', '-std=c++11') and not conf.AddOptionalFlag('.cc', 'CXXFLAGS', '-std=c++0x'):
    print("Your compiler is too old.")
    Exit(1)

if not conf.CheckCXXHeader('list'):
    print("C++ STL seems broken.")
    Exit(1)

if not 'cpu' in ARGUMENTS:
    opt_cpu = 'corei7'
else:
    opt_cpu = ARGUMENTS['cpu']

conf.AddOptionalFlag('.c', 'CCFLAGS',   "-march=%s" % opt_cpu )
conf.AddOptionalFlag('.c', 'LINKFLAGS', "-march=%s" % opt_cpu )

conf.AddOptionalFlag('.c', 'CCFLAGS', '-Wall')
#conf.AddOptionalFlag('.cc', 'CXXFLAGS', '-Weffc++')
conf.AddOptionalFlag('.c', 'CCFLAGS',   '-ffunction-sections')
conf.AddOptionalFlag('.c', 'LINKFLAGS', '-Wl,--gc-sections' )
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

common_objs = [host_env.Object(f) for f in Glob('switch/*.cc')]

## Programs
ts = host_env.Program('sv3', ['sv3.cc'] + common_objs)
# Clean leftover core files as well
Clean(ts, Glob("core.*"))

ts = host_env.Program('sv3-remote', ['sv3-remote.cc'] + common_objs)

# Tests

host_env.Program('test/checksums', ['test/checksums.cc'] + common_objs)
Command('test/checksums.log', ['test/checksums'], '! $SOURCE | tee $TARGET | grep -q FAILED')

host_env.Program('test/etherhash', ['test/etherhash.cc'] + common_objs)
host_env.Command('test/etherhash.log', ['test/etherhash' ], '$SOURCE | tee $TARGET')

if pcap_is_available:
    host_pcap_env.Program('test/packets', ['test/packets.cc'] + common_objs)
    Command('test/packets-ipv4-tcp.log', ['test/packets', 'test/data/ipv4-tcp.pcap' ], '! ${SOURCES[0]} ${SOURCES[1]} | tee $TARGET | grep -q wrong')
    Command('test/packets-ipv6-tcp.log', ['test/packets', 'test/data/ipv6-tcp.pcap' ], '! ${SOURCES[0]} ${SOURCES[1]} | tee $TARGET | grep -q wrong')
else:
    print("Not building test/packets! Not running tests!")

# EOF
