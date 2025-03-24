# Linker script for PE.

if test -z "${RELOCATEABLE_OUTPUT_FORMAT}"; then
  RELOCATEABLE_OUTPUT_FORMAT=${OUTPUT_FORMAT}
fi

# We can't easily and portably get an unquoted $ in a shell
# substitution, so we do this instead.
# Sorting of the .foo$* sections is required by the definition of
# grouped sections in PE.
# Sorting of the file names in R_IDATA is required by the
# current implementation of dlltool (this could probably be changed to
# use grouped sections instead).
if test "${RELOCATING}"; then
  R_TEXT='*(SORT(.text$*))'
  R_DATA='*(SORT(.data$*))'
  R_RDATA='*(SORT(.rdata$*))'
  R_IDATA='
    SORT(*)(.idata$2)  /* here we sort by filename! */
    SORT(*)(.idata$3)
    /* These zeroes mark the end of the import list.  */
    LONG (0); LONG (0); LONG (0); LONG (0); LONG (0);
    SORT(*)(.idata$4)
    SORT(*)(.idata$5)
    SORT(*)(.idata$6)
    SORT(*)(.idata$7)'
  R_CRT='*(SORT(.CRT$*))'
  R_RSRC='*(SORT(.rsrc$*))'
else
  R_TEXT=
  R_DATA=
  R_RDATA=
  R_IDATA=
  R_CRT=
  R_RSRC=
fi

# if DYNAMIC_LINKING [
# // Note XXX below; needs work. 
LINKERSECTS="${RELOCATING-0} ${RELOCATXXX+\(NOLOAD\)} ${RELOCATING+ BLOCK(__section_alignment__) }"
# ] 

cat <<EOF
${RELOCATING+OUTPUT_FORMAT(${OUTPUT_FORMAT})}
${RELOCATING-OUTPUT_FORMAT(${RELOCATEABLE_OUTPUT_FORMAT})}
${OUTPUT_ARCH+OUTPUT_ARCH(${OUTPUT_ARCH})}

${LIB_SEARCH_DIRS}

ENTRY(${ENTRY})

/* if DYNAMIC_LINKING [ */
/* Not sure yet */
${RELOCATING+/* Do we need any of these for elf?
   __DYNAMIC = 0; ${STACKZERO+${STACKZERO}} ${SHLIB_PATH+${SHLIB_PATH}}  */}
${RELOCATING+${EXECUTABLE_SYMBOLS}}
/* end DYNAMIC_LINKING ] */

SECTIONS
{
  .text ${RELOCATING+ __section_alignment__ } :
  {
    ${RELOCATING+ *(.init)}
    *(.text)
    ${R_TEXT}
    *(.glue_7t)
    *(.glue_7)
    ${RELOCATING+. = ALIGN(4);}
 
    /* collect constructors only for final links */
    ${RELOCATING+
        LONG (-1)
        KEEP (*(.ctor_head))
	KEEP (*(SORT(.ctors.*)))   /* Here we sort by section name! */
        KEEP (*(.ctors))
        KEEP (*(.ctor))
        LONG (0)

        LONG (-1)
        KEEP (*(.dtor_head))
	KEEP (*(SORT(.dtors.*)))
        KEEP (*(.dtors))
        KEEP (*(.dtor))
        LONG (0)
    }
    ${RELOCATING+ *(.fini)}
    /* ??? Why is .gcc_exc here?  */
    ${RELOCATING+ *(.gcc_exc)}
    ${RELOCATING+ etext = .;}
    *(.gcc_except_table)
  }

  /* The Cygwin32 library uses a section to avoid copying certain data
     on fork.  This used to be named ".data$nocopy".  The linker used
     to include this between __data_start__ and __data_end__, but that
     breaks building the cygwin32 dll.  Instead, we name the section
     ".data_cygwin_nocopy" and explictly include it after __data_end__. */

  .data ${RELOCATING+BLOCK(__section_alignment__)} : 
  {
    ${RELOCATING+__data_start__ = . ;}
    *(.data)
    *(.data2)
    ${R_DATA}
    ${RELOCATING+__data_end__ = . ;}
    ${RELOCATING+*(.data_cygwin_nocopy)}
  }

  .rdata ${RELOCATING+BLOCK(__section_alignment__)} :
  {
    *(.rdata)
    ${R_RDATA}
    *(.eh_frame)
    /* DYNAMIC_LINKING [ */
    *(.hash)
    *(.interp)
    /* ] */
    ${RELOCATING+___RUNTIME_PSEUDO_RELOC_LIST__ = .;}
    ${RELOCATING+__RUNTIME_PSEUDO_RELOC_LIST__ = .;}
    *(.rdata_runtime_pseudo_reloc)
    ${RELOCATING+___RUNTIME_PSEUDO_RELOC_LIST_END__ = .;}
    ${RELOCATING+__RUNTIME_PSEUDO_RELOC_LIST_END__ = .;}
  }

  .pdata ${RELOCATING+BLOCK(__section_alignment__)} :
  {
    *(.pdata)
  }

/* if DYNAMIC_LINKING [ */
  .got ${RELOCATING-0} ${RELOCATING+ BLOCK(__section_alignment__) } :
  { 
     *(.got.plt) 
     *(.got)
  }
/* end DYNAMIC_LINKING ] */

/* .idata must precede bss so file and code offsets remain the same for .sos */
/* (At least for now... using Ldr* routines may fix.) */
  .idata ${RELOCATING+BLOCK(__section_alignment__)} :
  {
    /* This cannot currently be handled with grouped sections.
	See pe.em:sort_sections.  */
    ${R_IDATA}
  }

  .bss ${RELOCATING+BLOCK(__section_alignment__)} :
  {
    ${RELOCATING+__bss_start__ = . ;}
/* DYNAMIC_LINKING */
    *(.dynbss)
/* end DYNAMIC_LINKING */
    *(.bss)
    *(COMMON)
    ${RELOCATING+__bss_end__ = . ;}
  }

  .edata ${RELOCATING+BLOCK(__section_alignment__)} :
  {
    *(.edata)
  }

  /DISCARD/ :
  {
    *(.debug\$S)
    *(.debug\$T)
    *(.debug\$F)
    *(.drectve)
  }

  .CRT ${RELOCATING+BLOCK(__section_alignment__)} :
  { 					
    ${R_CRT}
  }

  .endjunk ${RELOCATING+BLOCK(__section_alignment__)} :
  {
    /* end is deprecated, don't use it */
    ${RELOCATING+ end = .;}
    ${RELOCATING+ _end = .;}
    ${RELOCATING+ __end__ = .;}
  }

/* DYNAMIC_LINKING [  // XXX below, rela??? */
  /* // ??? .dynamic ${RELOCATING-0} ${RELOCATXXX+"(NOLOAD)"} : { */
  .dynamic $LINKERSECTS: {
    *(.dynamic)
  }

  .dynsym $LINKERSECTS : { 
    *(.dynsym)	
  }

  .dynstr $LINKERSECTS : { 
    *(.dynstr)
  }

  .gnu.version $LINKERSECTS : {
    *(.gnu.version)
  }

  .gnu.version_d $LINKERSECTS : {
    *(.gnu.version_d)
  }

  .gnu.version_r $LINKERSECTS : {
    *(.gnu.version_r)
  }

  .rel.dyn $LINKERSECTS :
  { 
    *(.rel.internal)
    *(.rel.got)
    *(.rel.plt)
  }

  .rela.dyn $LINKERSECTS :
  { 
    *(.rela.*)
  }

  .init $LINKERSECTS : { 
    *(.init)
  } =${NOP-0}

/* end DYNAMIC_LINKING ] */

  .rsrc ${RELOCATING+BLOCK(__section_alignment__)} :
  { 					
    *(.rsrc)
    ${R_RSRC}
  }

  .reloc ${RELOCATING+BLOCK(__section_alignment__)} :
  { 					
    *(.reloc)
  }

  .stab ${RELOCATING+BLOCK(__section_alignment__)} ${RELOCATING+(NOLOAD)} :
  {
    [ .stab ]
  }

  .stabstr ${RELOCATING+BLOCK(__section_alignment__)} ${RELOCATING+(NOLOAD)} :
  {
    [ .stabstr ]
  }

}
EOF
