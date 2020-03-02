#!/usr/bin/env python3
import argparse
import subprocess
from abc import ABCMeta, abstractmethod
import sys

from pyfastogt import system_info, build_utils

# Script for building environment on clean machine

if sys.version_info < (3, 5, 0):  # meson limitations
    print('Tried to start script with an unsupported version of Python. build_env requires Python 3.5.0 or greater')
    sys.exit(1)


class OperationSystem(metaclass=ABCMeta):
    @abstractmethod
    def get_required_exec(self) -> list:
        pass

    @abstractmethod
    def get_build_exec(self) -> list:
        pass


class Debian(OperationSystem):
    def get_required_exec(self) -> list:
        return ['git', 'yasm', 'nasm', 'gcc', 'g++', 'make', 'ninja-build', 'cmake', 'python3-pip']

    def get_build_exec(self) -> list:
        return ['autoconf', 'automake', 'libtool', 'pkg-config', 'libudev-dev', 'libmongoc-dev', 'libbson-dev', 'libssl-dev']


class RedHat(OperationSystem):
    def get_required_exec(self) -> list:
        return ['git', 'yasm', 'nasm', 'gcc', 'gcc-c++', 'make', 'ninja-build', 'cmake', 'python3-pip']

    def get_build_exec(self) -> list:
        return ['autoconf', 'automake', 'libtool', 'pkgconfig', 'libudev-devel', 'libmongoc-devel', 'openssl-devel']


class Arch(OperationSystem):
    def get_required_exec(self) -> list:
        return ['git', 'yasm', 'nasm', 'gcc', 'make', 'ninja', 'cmake', 'python3-pip']

    def get_build_exec(self) -> list:
        return ['autoconf', 'automake', 'libtool', 'pkgconfig', 'udev', 'libmongoc-dev', 'openssl']


class FreeBSD(OperationSystem):
    def get_required_exec(self) -> list:
        return ['git', 'yasm', 'nasm', 'gcc', 'make', 'ninja', 'cmake', 'python3-pip']

    def get_build_exec(self) -> list:
        return ['autoconf', 'automake', 'libtool', 'pkgconfig', 'libudev-devd', 'libmongoc-dev', 'openssl']


class Windows64(OperationSystem):
    def get_required_exec(self) -> list:
        return ['git', 'mingw-w64-x86_64-yasm', 'mingw-w64-x86_64-nasm', 'mingw-w64-x86_64-gcc', 'make',
                'mingw-w64-x86_64-ninja', 'mingw-w64-x86_64-cmake', 'mingw-w64-x86_64-python3-pip',
                'mingw-w64-x86_64-pkg-config']

    def get_build_exec(self) -> list:
        return []


class Windows32(OperationSystem):
    def get_required_exec(self) -> list:
        return ['git', 'mingw-w64-i686-yasm', 'mingw-w64-i686-nasm', 'mingw-w64-i686-gcc', 'make',
                'mingw-w64-i686-ninja', 'mingw-w64-i686-cmake', 'mingw-w64-i686-python3-pip', 'mingw-w64-i686-pkg-config']

    def get_build_exec(self) -> list:
        return []


class MacOSX(OperationSystem):
    def get_required_exec(self) -> list:
        return ['git', 'yasm', 'nasm', 'make', 'ninja', 'cmake', 'python3-pip']

    def get_build_exec(self) -> list:
        return ['autoconf', 'automake', 'libtool', 'pkgconfig']


class BuildRequest(build_utils.BuildRequest):
    def __init__(self, platform, arch_name, dir_path, prefix_path):
        build_utils.BuildRequest.__init__(self, platform, arch_name, dir_path, prefix_path)

    def get_system_libs(self, repo_build=False):
        platform = self.platform_
        platform_name = platform.name()
        ar = platform.architecture()
        dep_libs = []

        current_system = None
        if platform_name == 'linux':
            distribution = system_info.linux_get_dist()
            if distribution == 'DEBIAN':
                current_system = Debian()
                dep_libs.extend(current_system.get_required_exec())
                dep_libs.extend(current_system.get_build_exec())
            elif distribution == 'RHEL':
                current_system = RedHat()
                dep_libs.extend(current_system.get_required_exec())
                dep_libs.extend(current_system.get_build_exec())
            elif distribution == 'ARCH':
                current_system = Arch()
                dep_libs.extend(current_system.get_required_exec())
                dep_libs.extend(current_system.get_build_exec())
        elif platform_name == 'freebsd':
            current_system = FreeBSD()
            dep_libs.extend(current_system.get_required_exec())
            dep_libs.extend(current_system.get_build_exec())
        elif platform_name == 'macosx':
            current_system = MacOSX()
            dep_libs.extend(current_system.get_required_exec())
            dep_libs.extend(current_system.get_build_exec())
        elif platform_name == 'windows':
            if ar.bit() == 64:
                current_system = Windows64()
                dep_libs.extend(current_system.get_required_exec())
            elif ar.bit() == 32:
                current_system = Windows32()
                dep_libs.extend(current_system.get_required_exec())

        if not current_system:
            raise NotImplementedError("Unknown platform '%s'" % platform_name)

        return dep_libs

    def install_system(self):
        dep_libs = self.get_system_libs()
        for lib in dep_libs:
            self._install_package(lib)

        # post install step
        platform = self.platform()
        platform_name = platform.name()
        if platform_name == 'linux':
            distribution = system_info.linux_get_dist()
            if distribution == 'RHEL':
                subprocess.call(['ln', '-sf', '/usr/bin/ninja-build', '/usr/bin/ninja'])


def str2bool(v):
    if isinstance(v, bool):
        return v
    if v.lower() in ('yes', 'true', 't', 'y', '1'):
        return True
    elif v.lower() in ('no', 'false', 'f', 'n', '0'):
        return False
    else:
        raise argparse.ArgumentTypeError('Boolean value expected.')


if __name__ == "__main__":
    host_os = system_info.get_os()
    arch_host_os = system_info.get_arch_name()

    parser = argparse.ArgumentParser(prog='build_env', usage='%(prog)s [options]')
    # system
    system_grp = parser.add_mutually_exclusive_group()
    system_grp.add_argument('--with-system', help='build with system dependencies (default)', dest='with_system',
                            action='store_true', default=True)
    system_grp.add_argument('--without-system', help='build without system dependencies', dest='with_system',
                            action='store_false', default=False)

    # json-c
    jsonc_grp = parser.add_mutually_exclusive_group()
    jsonc_grp.add_argument('--with-json-c', help='build json-c (default, version: git master)', dest='with_jsonc',
                           action='store_true', default=True)
    jsonc_grp.add_argument('--without-json-c', help='build without json-c', dest='with_jsonc', action='store_false',
                           default=False)

    # libev
    libev_grp = parser.add_mutually_exclusive_group()
    libev_grp.add_argument('--with-libev', help='build libev (default, version: git master)', dest='with_libev',
                           action='store_true', default=True)
    libev_grp.add_argument('--without-libev', help='build without libev', dest='with_libev', action='store_false',
                           default=False)

    # common
    common_grp = parser.add_mutually_exclusive_group()
    common_grp.add_argument('--with-common', help='build common (default, version: git master)', dest='with_common',
                            action='store_true', default=True)
    common_grp.add_argument('--without-common', help='build without common', dest='with_common',
                            action='store_false',
                            default=False)

    # fastotv_protocol
    fastotv_protocol_grp = parser.add_mutually_exclusive_group()
    fastotv_protocol_grp.add_argument('--with-fastotv-protocol',
                                      help='build fastotv_protocol (default, version: git master)',
                                      dest='with_fastotv_protocol',
                                      action='store_true', default=True)
    fastotv_protocol_grp.add_argument('--without-fastotv-protocol', help='build without fastotv_protocol',
                                      dest='with_fastotv_protocol',
                                      action='store_false',
                                      default=False)

    # other
    parser.add_argument('--platform', help='build for platform (default: {0})'.format(host_os), default=host_os)
    parser.add_argument('--architecture', help='architecture (default: {0})'.format(arch_host_os),
                        default=arch_host_os)
    parser.add_argument('--prefix', help='prefix path (default: None)', default=None)

    parser.add_argument('--install-other-packages',
                        help='install other packages (--with-system, --with-tools --with-meson --with-jsonc --with-libev) (default: True)',
                        dest='install_other_packages', type=str2bool, default=True)
    parser.add_argument('--install-fastogt-packages',
                        help='install FastoGT packages (--with-common --with-fastotv-protocol) (default: True)',
                        dest='install_fastogt_packages', type=str2bool, default=True)

    argv = parser.parse_args()

    arg_platform = argv.platform
    arg_prefix_path = argv.prefix
    arg_architecture = argv.architecture
    arg_install_other_packages = argv.install_other_packages
    arg_install_fastogt_packages = argv.install_fastogt_packages

    request = BuildRequest(arg_platform, arg_architecture, 'build_' + arg_platform + '_env', arg_prefix_path)
    if argv.with_system and arg_install_other_packages:
        request.install_system()

    if argv.with_jsonc and arg_install_other_packages:
        request.build_jsonc()
    if argv.with_libev and arg_install_other_packages:
        request.build_libev()
    if argv.with_common and arg_install_fastogt_packages:
        request.build_common()

    if argv.with_fastotv_protocol and arg_install_fastogt_packages:
        request.build_fastotv_protocol()
