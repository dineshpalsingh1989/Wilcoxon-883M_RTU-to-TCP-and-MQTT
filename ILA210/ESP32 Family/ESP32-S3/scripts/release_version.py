"""Set firmware release identity before PlatformIO creates compilation environments."""
import os
import re
Import("env")
tag = os.environ.get("GITHUB_REF_NAME", "")
if os.environ.get("GITHUB_REF_TYPE") == "tag" and re.fullmatch(r"v\d+\.\d+\.\d+(-beta\.\d+)?", tag):
    env.Append(CPPDEFINES=[("ILA_RELEASE_VERSION", env.StringifyMacro(tag[1:]))])
