
APPNAME = 'bjne'
VERSION = '0.2.1'

srcdir = '.'
blddir = 'build'

def set_options(opt):
    opt.tool_options('compiler_cxx')
    opt.recurse('src')

def configure(conf):
    conf.check_tool('compiler_cxx')
    conf.env.CXXFLAGS += ['-O2', '-Wall', '-g']
    conf.recurse('src')

def build(bld):
    bld.recurse('src')

def shutdown(ctx):
    pass
