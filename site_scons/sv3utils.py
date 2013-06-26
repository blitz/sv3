from SCons.Script import *

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

def CheckPKGConfig(context, version):
     context.Message( 'Checking for pkg-config... ' )
     ret = context.TryAction('pkg-config --atleast-pkgconfig-version=%s' % version)[0]
     context.Result( ret )
     return ret

def CheckPKG(context, name):
     context.Message( 'Checking for %s... ' % name )
     ret = context.TryAction('pkg-config --exists \'%s\'' % name)[0]
     context.Result( ret )
     return ret

# EOF
