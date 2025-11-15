#!/bin/python

from SCons.Script import DefaultEnvironment # type: ignore
env = DefaultEnvironment()

def before_build_spiffs(source, target, env):
    env.Execute("gulp buildfs")

#env.AddPreAction(".pioenvs/%s/littlefs.bin" % env['PIOENV'], before_build_spiffs)
env.AddPreAction( '$BUILD_DIR/littlefs.bin', before_build_spiffs)
