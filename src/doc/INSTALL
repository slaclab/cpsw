# Installation Instructions for the CPSW Package

## Copyright Notice
This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
file found in the top-level directory of this distribution and
[at](https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html).

No part of CPSW, including this file, may be copied, modified, propagated, or
distributed except according to the terms contained in the LICENSE.txt file.

## 1. Makefiles

CPSW comes with its own makefile system which borrows ideas
from the EPICS build system.

The makefiles support building multiple target architectures,
shared- and static libraries and programs.

The makefiles also support building applications which are
layered on top of CPSW (and do not share the CPSW source directory
but may be located at an arbitrary location in the filesystem).
Consult 'src/makefile.template' for further information.

The makefiles consist of (you can skip this explanation if you
want to proceed with a vanilla build)

 - `src/rules.mak`:
     generic rules; should NOT be modified; lives in the CPSW source
     directory (included by derived libraries and applications).

 - `src/defs.mak`:
     generic definitions; should NOT be modified; lives in the CPSW
     source directory (included by derived libraries and applications). 

 - `config.mak`:
     site-specific settings of certain variables that affect the build
     (AND the build of derived libraries and apps); lives in the CPSW
     top directory.

 - `config.local.mak`:
     local tweaks of variables that affect the build (AND the build of
     derived libraries and apps); lives in the CPSW top directory.
     SEE BELOW for what this is for.

 - `src/makefile`:
     Defines application/library-specific variables, i.e., mostly
     lists the targets to build and the files that are required
     for them (see makefile.template). Can also contain app-specific
     rules. Lives in the application source directory. Binaries
     are built under 'O.<architecture>' directories.

 - `src/local.mak`:
     Local tweaks which are application/library-specific; lives
     in the application source directory.

## 2. `config.mak`

This file may be edited to define

 1. the desired set of target architectures
 2. the name and location of the compiler toolchain for each
        target architecture 
 3. the location of the dependent 'boost' and 'yaml-cpp' libraries.
 4. whether to build a static and/or dynamic CPSW library
 5. the installation location

The aforementioned definitions may also be overridden from a file
`config.local.mak`: in many cases `config.mak` is maintained in git
and reflects the site-specific settings. However, a developer may
occasionally want to build the package in a different environment
(e.g., on a private laptop).
This is when `config.local.mak` is useful as it allows the developer
to define a 'private' configuration to override the 'normal' one.
`config.local.mak` should not be maintained in 'git' but be reserved
for such special cases.

In the common case of

 - native-only build - no cross-compilation
 - 'boost' and 'yaml-cpp' packages natively available (e.g., installed
   by your distro's package manager)

there is no need for any settings in `config.mak` -- an empty file will
do. However, you may want to define the installation location (5. above).

The following section discusses the settings 1..5 above:

### 2.1 Select target architectures:

Each desired target architecture must be added to the `ARCHES` variable
using the `+=` operator, e.g.:

        ARCHES += buildroot-2016.11.1-x86_64

Binaries for all architectures are build in a `O.<arch>` subdirectory,
e.g., `O.linuxRT-x86_64`. Installation (`make install`) also deposits
binaries and libraries into a architecture-specific subdirectory, e.g.,

        $(INSTALL_DIR)/buildroot-2016.11.1-x86_64/lib

Note that the host architecture is `host` by default. This name may
be changed by setting the `HARCH` variable, e.g.,

        HARCH = linux-x86_64

Another example, for some systems with multiple linux distributions:

        HARCH = rhel6-x86_64

`HARCH` is automatically added to `ARCHES`, so there is no need to do so.

#### 2.1.1 Target Architecture-Specific Makefile Variables

This subsection contains detailed information - you may skip it if you
are just want to build CPSW.

An entire set of variables (look for `ARCHSPECIFIC_VARS` in `defs.mak`)
is defined in a way so that during expansion a few variants are
tried:

        FOO = $(or $(FOO_$(TARNM)),$(FOO_default))

This means that when building for architecture `bar`, `FOO` expands
to the value of `FOO_bar` if such a variable is defined (non-empty).
If `$(FOO_bar)` is empty then `FOO` evaluates to the value of
`FOO_default`. 

This scheme allows for precise control over the value `FOO` should
have when building for a specific architecture.

When working with external packages then usually the build process
requires access to headers and libraries. The makefiles provide
a mechanism to define variables which point to architecture-specific
locations where such headers and libraries can be found.

Respective variables are defined, based on stems listed in
`ARCHSPECIFIC_LIBVARS`. For each `stem` listed there, definitions

        stem_DIR=$(or $(stem_DIR_$(TARNM)),$(stem_DIR_default))
        steminc_DIR=$(or $(steminc_DIR_$(TARNM),$(steminc_DIR_default),$(addsuffix /include,$(stem_DIR))))
        stemlib_DIR=$(or $(stemlib_DIR_$(TARNM),$(stemlib_DIR_default),$(addsuffix /lib,$(stem_DIR))))

are emitted. These definitions cause e.g., `$(steminc_DIR)` to evaluate
to the first non-empty value of

  1. `steminc_DIR_<arch>`
  2. `steminc_DIR_default`
  3. `stem_DIR_<arch>/include`
  4. `stem_DIR_default/include`

It is possible to add your own variables and stems to `ARCHSPECIFIC_VARS`
and `ARCHSPECIFIC_LIBVARS`, respectively. You should do so in your 
`makefile` *before* including `defs.mak`.

### 2.2 Compiler Toolchain

The makefiles offer fine-grained control of the selection of tools.
Only the most common cases are covered here.

Tools are picked in the following way:

        <architecture-specific-prefix><toolname>

Each of these components can be fine-tuned. The default toolnames are
`gcc`, `ld`, `install` etc.

The architecture-specific prefix is (by default) empty for the host
and identical to the architecture with a `-` appended for cross-builds.

E.g., the compiler for the `linuxRT-x86_64` architecture would be

        linuxRT-x86_64-gcc

If the such a compiler can indeed be found in the current `PATH` then
no special setting in `config.mak` is required. If the tool-prefix
requires renaming then this can be achieved by setting

        CROSS_<archname>=<architecture-specific-prefix>

Since makefile variables must not contain any `-` characters all
hyphens present in an architecture must be replaced by underscores
(`_`) on the left hand side of the above definition.

E.g., if the compiler for `linuxRT-x86_64` is really named `x86_64-linux-g++`
then remapping of the tool-prefix woud be defined as follows:

        CROSS_linuxRT_x86_64=x86_64-linux-

Note that this requires the compiler to be found in the `PATH`. If
this is not desired then an absolute path prefix may simply be added
to `CROSS_<archname>`.

        CROSS_linuxRT_x86_64=/path/to/tools/x86_64-linux-

#### 2.2.1 C++11 Support
CPSW can be built with a C++-11 compiler. This has the advantage of
removing the dependency on `boost` for _applications_. I.e., CPSW
itself still requires some features provided by `boost` but these
are not exported to the API. The variables

        USE_CXX11_default
        USE_CXX11_<architecture>

can be set to `YES` or `NO`, respectively in order to enable or
disable the use of C++-11.

### 2.3 Dependent 'boost' and 'yaml-cpp' Packages

If the 'boost' and 'yaml-cpp' packages are not installed in a standard
location then the following variables must be defined and pointed to
the correct location(s) for include files and libraries, respectively.

        boostinc_DIR=
        boostlib_DIR=
        yaml_cppinc_DIR=
        yaml_cpplib_DIR=

If different architectures use different locations then you may set

        boostinc_DIR_xxx=/path/for/arch/xxx
        boostinc_DIR_default=/path/for/others

If includes and libraries are in `include` and `lib` subdirectories
under a common parent then setting `boost_DIR` and `yaml_cpp_DIR` or their
arch-specific variants is easier.

Assume you have yaml-cpp installed natively (for the host) and
under `/usr/local/yaml-cpp/0.5.3/` for the `linuxRT-x86_64` architecture
(with includes under `/usr/local/yaml-cpp/0.5.3/include` and libraries
under `/usr/local/yaml-cpp/0.5.3/lib`).

You could then say

        # no need to do anything for the host since it is installed natively
        yaml_cpp_DIR_linuxRT_x86_64=/usr/local/yaml-cpp/0.5.3/

Dont' forget to substitute `-` with `_` in gnumake variable names.

### 2.4 Building Static and Shared Libraries.

The makefile system can build either static or shared libraries or
both. You can tune by explicitly setting

        WITH_SHARED_LIBRARIES=YES # or 'NO'
        WITH_STATIC_LIBRARIES=NO  # or 'YES'

*Note*: It is recommended to build shared libraries (exclusively).
Otherwise, run-time loading of drivers/classes may not work unless
the application is linked with special linker flags. Also, the
python bindings require shared libraries.

### 2.5 Python Bindings

CPSW comes with python bindings for python 2.7 or 3.4 (other versions
may work but have not been tested). You need the `boost_python` library
and headers for building the python bindings as well as the python
headers. Defining the location of `boost` has already been discussed.

Analogous variables for python (headers) are employed:

        $(pyinc_DIR_<architecture>)
        $(pyinc_DIR_default)
        $(py_DIR_<architecture>)/include
        $(py_DIR_default)/include

are searched in that order. The default name for the `boost_python` 
library (without pre- and suffix) is `boost_python`. If you need
to change this name (e.g., on ubuntu the respective library for
python3.4 is called `boost_python-py34`) then you can modify

        BOOST_PYTHON_LIB_<architecture>
        BOOST_PYTHON_LIB_default

Finally, you may have to define a variant of `WITH_PYCPSW` to `YES` or `NO`
in order to instruct the makefiles to actually build the python bindings:

        WITH_PYCPSW_<architecture> = NO  # or YES
        WITH_PYCPSW_default        = YES # or NO

However, the default setting already tries to 'guess the right thing':
it will cause python bindings to be built if and only if

 1. `pyinc_DIR` evaluates to a nonempty string for the target architecture
    (i.e., any of `pyinc_DIR_<arch>`, `pyinc_DIR_default`, `py_DIR_<arch>`,
    `py_DIR_default` is set).
 2. `WITH_SHARED_LIBRARIES` evaluates to `YES` for the target architecture

*Note*: the python bindings require cpsw and dependent *shared libraries*.

The resulting python module is called `pycpsw` and may be imported as
such into python. Note that the dynamic/run-time linker must be able to locate
`libcpsw` and other dependent libraries, e.g., in the environment variable
`LD_LIBRARY_PATH` or by some other means (consult `man ld.so` for more
information).

### 2.6 The Installation Location

`make install` installs headers into

        $(INSTALL_DIR)/<ARCH>/include

libraries into

        $(INSTALL_DIR)/<architecture>/lib/

binaries into

        $(INSTALL_DIR)/<architecture>/bin/

and documentation into

        $(INSTALL_DIR)/<architecture>/doc/

## 3. Running `make`

The makefiles understand the standard goals `all`, `clean`, `install`
and `test` with `all` being the default goal.

However, since the makefile system is designed to build recursively for
multiple target architectures and subsystem directories it is possible
and sometimes necessary to specify goals in a more fine-grained way.

Any target for which a rule is defined (either in `rules.mak` or in the
`makefile`) - when used as a 'goal' - must be supplied with a prefix
which indicates the set of architectures for which the goal should
be built. The most common cases are

        multi-<goal>      => builds <goal> for all architectures
        sub-<arch>@<goal> => builds <goal> for <arch> only

Thus, e.g., the aforementioned `all` target is actually an alias for
`multi-build`. Since the test programs can only be executed by make
on the host system, the 'test' goal is actually an alias for
`sub-$(HARCH)@run_tests`.

For building the CPSW package it is sufficient to execute

        make

and optionally 

        make install

Note that the latter requires `INSTALL_DIR` to be defined either in
config.mak or on the command line.

For easy de-installation a 'make uninstall' target is available.

## 4. Further Information

The `config.mak` file contains comments which explain in a little
more detail how to configure the package.

`makefile.template` explains how to use the CPSW makefiles for
building software which are layered on top of CPSW. Of course, the
use of CPSW makefiles for this purpose is entirely optional.
