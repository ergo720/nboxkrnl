#ergo720                Copyright (c) 2026

import subprocess

git_hash = subprocess.check_output(["git", "rev-parse", "HEAD"]).strip().decode('utf-8')
with open("../build/kernel_version.hpp", "w+") as kernel_version_temp_f:
    kernel_header_temp_str = "#pragma once\n\n#define _NBOXKRNL_VERSION \"{0}\"\n".format(git_hash)
    kernel_version_temp_f.write(kernel_header_temp_str)
    try:
        with open("../nboxkrnl/kernel_version.hpp", "r+") as kernel_version_f:
            kernel_header_str = kernel_version_f.read()
            if kernel_header_str != kernel_header_temp_str:
                kernel_version_f.seek(0)
                kernel_version_f.write(kernel_header_temp_str)
    except FileNotFoundError:
        with open("../nboxkrnl/kernel_version.hpp", "w") as kernel_version_f:
            kernel_version_f.write(kernel_header_temp_str)
