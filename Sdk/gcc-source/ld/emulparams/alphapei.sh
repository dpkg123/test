ARCH=nt_alpha
SCRIPT_NAME=pe
OUTPUT_FORMAT="pei-alpha"
LINK_FORMAT="pe-alpha" # for link -r
TEMPLATE_NAME=pe
ENTRY="__PosixProcessStartup"
EXECUTABLE_NAME=a.out
INITIAL_SYMBOL_CHAR=
OVERRIDE_SECTION_ALIGNMENT=0x2000
GENERATE_SHLIB_SCRIPT=yes
DYNAMIC_LINK=false # as a default
if [ -f LINKER_DEFAULT_OVERRIDE ]
then
    . LINKER_DEFAULT_OVERRIDE
fi
