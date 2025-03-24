#define TE_PE_DYN /* PE with dynamic linking (UNIX shared lib) support */
#define TE_PE
#define LEX_AT 1 /* can have @'s inside labels */
#define RECOGNIZE_AT_GOT_ANYWAY   /* @GOTOFF, etc, still apply! */
#define LEX_QM 3 /* can have ?'s in or begin labels */

/* The PE format supports long section names.  */
#define COFF_LONG_SECTION_NAMES

#define GLOBAL_OFFSET_TABLE_NAME "__GLOBAL_OFFSET_TABLE_"

/* Both architectures use these.  */
#ifndef LOCAL_LABELS_FB
#define LOCAL_LABELS_FB 1
#endif

#define STRICT_PE_FORMAT

#include "obj-format.h"
