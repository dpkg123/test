/* Support for the generic parts of PE/PEI; the common executable parts.
   Copyright 1995, 1996, 1997 Free Software Foundation, Inc.
   Written by Cygnus Support.

This file is part of BFD, the Binary File Descriptor library.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/*
Most of this hacked by  Steve Chamberlain,
			sac@cygnus.com

PE/PEI rearrangement (and code added): Donn Terry
				       Softway Systems, Inc.
*/

/* Hey look, some documentation [and in a place you expect to find it]!

   The main reference for the pei format is "Microsoft Portable Executable
   and Common Object File Format Specification 4.1".  Get it if you need to
   do some serious hacking on this code.

   Another reference:
   "Peering Inside the PE: A Tour of the Win32 Portable Executable
   File Format", MSJ 1994, Volume 9.

   The *sole* difference between the pe format and the pei format is that the
   latter has an MSDOS 2.0 .exe header on the front that prints the message
   "This app must be run under Windows." (or some such).
   (FIXME: Whether that statement is *really* true or not is unknown.
   Are there more subtle differences between pe and pei formats?
   For now assume there aren't.  If you find one, then for God sakes
   document it here!)

   The Microsoft docs use the word "image" instead of "executable" because
   the former can also refer to a DLL (shared library).  Confusion can arise
   because the `i' in `pei' also refers to "image".  The `pe' format can
   also create images (i.e. executables), it's just that to run on a win32
   system you need to use the pei format.

   FIXME: Please add more docs here so the next poor fool that has to hack
   on this code has a chance of getting something accomplished without
   wasting too much time.
*/

#include "bfd.h"
#include "sysdep.h"
#include "libbfd.h"
#include "coff/internal.h"
/* NOTE: it's strange to be including an architecture specific header
   in what's supposed to be general (to PE/PEI) code.  However, that's
   where the definitions are, and they don't vary per architecture
   within PE/PEI, so we get them from there. */
#include "coff/i386.h"
#include "coff/pe.h"
#include "libcoff.h"

#include "libpei.h"

boolean in_reloc_p PARAMS((bfd *, reloc_howto_type *));

/**********************************************************************/

void
pei_swap_sym_in (abfd, ext1, in1)
     bfd            *abfd;
     PTR ext1;
     PTR in1;
{
  SYMENT *ext = (SYMENT *)ext1;
  struct internal_syment      *in = (struct internal_syment *)in1;

  if( ext->e.e_name[0] == 0) {
    in->_n._n_n._n_zeroes = 0;
    in->_n._n_n._n_offset = bfd_h_get_32(abfd, (bfd_byte *) ext->e.e.e_offset);
  }
  else {
#if SYMNMLEN != E_SYMNMLEN
    -> Error, we need to cope with truncating or extending SYMNMLEN!;
#else
    memcpy(in->_n._n_name, ext->e.e_name, SYMNMLEN);
#endif
  }

  in->n_value = bfd_h_get_32(abfd, (bfd_byte *) ext->e_value); 
  in->n_scnum = bfd_h_get_16(abfd, (bfd_byte *) ext->e_scnum);
  if (sizeof(ext->e_type) == 2){
    in->n_type = bfd_h_get_16(abfd, (bfd_byte *) ext->e_type);
  }
  else {
    in->n_type = bfd_h_get_32(abfd, (bfd_byte *) ext->e_type);
  }
  in->n_sclass = bfd_h_get_8(abfd, ext->e_sclass);
  in->n_numaux = bfd_h_get_8(abfd, ext->e_numaux);

#ifdef coff_swap_sym_in_hook
  coff_swap_sym_in_hook(abfd, ext1, in1);
#endif
}

unsigned int
pei_swap_sym_out (abfd, inp, extp)
     bfd       *abfd;
     PTR	inp;
     PTR	extp;
{
  struct internal_syment *in = (struct internal_syment *)inp;
  SYMENT *ext =(SYMENT *)extp;
  if(in->_n._n_name[0] == 0) {
    bfd_h_put_32(abfd, 0, (bfd_byte *) ext->e.e.e_zeroes);
    bfd_h_put_32(abfd, in->_n._n_n._n_offset, (bfd_byte *)  ext->e.e.e_offset);
  }
  else {
#if SYMNMLEN != E_SYMNMLEN
    -> Error, we need to cope with truncating or extending SYMNMLEN!;
#else
    memcpy(ext->e.e_name, in->_n._n_name, SYMNMLEN);
#endif
  }

  bfd_h_put_32(abfd,  in->n_value , (bfd_byte *) ext->e_value);
  bfd_h_put_16(abfd,  in->n_scnum , (bfd_byte *) ext->e_scnum);
  if (sizeof(ext->e_type) == 2)
    {
      bfd_h_put_16(abfd,  in->n_type , (bfd_byte *) ext->e_type);
    }
  else
    {
      bfd_h_put_32(abfd,  in->n_type , (bfd_byte *) ext->e_type);
    }
  bfd_h_put_8(abfd,  in->n_sclass , ext->e_sclass);
  bfd_h_put_8(abfd,  in->n_numaux , ext->e_numaux);

  return SYMESZ;
}

void
pei_swap_aux_in (abfd, ext1, type, class, indx, numaux, in1)
     bfd            *abfd;
     PTR 	      ext1;
     int             type;
     int             class;
     int	      indx;
     int	      numaux;
     PTR 	      in1;
{
  AUXENT    *ext = (AUXENT *)ext1;
  union internal_auxent *in = (union internal_auxent *)in1;

  switch (class) {
  case C_FILE:
    if (ext->x_file.x_fname[0] == 0) {
      in->x_file.x_n.x_zeroes = 0;
      in->x_file.x_n.x_offset = 
	bfd_h_get_32(abfd, (bfd_byte *) ext->x_file.x_n.x_offset);
    } else {
#if FILNMLEN != E_FILNMLEN
      -> Error, we need to cope with truncating or extending FILNMLEN!;
#else
      memcpy (in->x_file.x_fname, ext->x_file.x_fname, FILNMLEN);
#endif
    }
    return;


  case C_STAT:
#ifdef C_LEAFSTAT
  case C_LEAFSTAT:
#endif
  case C_HIDDEN:
    if (type == T_NULL) {
      in->x_scn.x_scnlen = GET_SCN_SCNLEN(abfd, ext);
      in->x_scn.x_nreloc = GET_SCN_NRELOC(abfd, ext);
      in->x_scn.x_nlinno = GET_SCN_NLINNO(abfd, ext);
      in->x_scn.x_checksum = bfd_h_get_32 (abfd,
					   (bfd_byte *) ext->x_scn.x_checksum);
      in->x_scn.x_associated =
	bfd_h_get_16 (abfd, (bfd_byte *) ext->x_scn.x_associated);
      in->x_scn.x_comdat = bfd_h_get_8 (abfd,
					(bfd_byte *) ext->x_scn.x_comdat);
      return;
    }
    break;
  }

  in->x_sym.x_tagndx.l = bfd_h_get_32(abfd, (bfd_byte *) ext->x_sym.x_tagndx);
#ifndef NO_TVNDX
  in->x_sym.x_tvndx = bfd_h_get_16(abfd, (bfd_byte *) ext->x_sym.x_tvndx);
#endif

  if (class == C_BLOCK || class == C_FCN || ISFCN (type) || ISTAG (class))
    {
      in->x_sym.x_fcnary.x_fcn.x_lnnoptr = GET_FCN_LNNOPTR (abfd, ext);
      in->x_sym.x_fcnary.x_fcn.x_endndx.l = GET_FCN_ENDNDX (abfd, ext);
    }
  else
    {
#if DIMNUM != E_DIMNUM
 #error we need to cope with truncating or extending DIMNUM
#endif
      in->x_sym.x_fcnary.x_ary.x_dimen[0] =
	bfd_h_get_16 (abfd, (bfd_byte *) ext->x_sym.x_fcnary.x_ary.x_dimen[0]);
      in->x_sym.x_fcnary.x_ary.x_dimen[1] =
	bfd_h_get_16 (abfd, (bfd_byte *) ext->x_sym.x_fcnary.x_ary.x_dimen[1]);
      in->x_sym.x_fcnary.x_ary.x_dimen[2] =
	bfd_h_get_16 (abfd, (bfd_byte *) ext->x_sym.x_fcnary.x_ary.x_dimen[2]);
      in->x_sym.x_fcnary.x_ary.x_dimen[3] =
	bfd_h_get_16 (abfd, (bfd_byte *) ext->x_sym.x_fcnary.x_ary.x_dimen[3]);
    }

  if (ISFCN(type)) {
    in->x_sym.x_misc.x_fsize = bfd_h_get_32(abfd, (bfd_byte *) ext->x_sym.x_misc.x_fsize);
  }
  else {
    in->x_sym.x_misc.x_lnsz.x_lnno = GET_LNSZ_LNNO(abfd, ext);
    in->x_sym.x_misc.x_lnsz.x_size = GET_LNSZ_SIZE(abfd, ext);
  }
}

unsigned int
pei_swap_aux_out (abfd, inp, type, class, indx, numaux, extp)
     bfd   *abfd;
     PTR 	inp;
     int   type;
     int   class;
     int   indx;
     int   numaux;
     PTR	extp;
{
  union internal_auxent *in = (union internal_auxent *)inp;
  AUXENT *ext = (AUXENT *)extp;

  memset((PTR)ext, 0, AUXESZ);
  switch (class) {
  case C_FILE:
    if (in->x_file.x_fname[0] == 0) {
      bfd_h_put_32(abfd, 0, (bfd_byte *) ext->x_file.x_n.x_zeroes);
      bfd_h_put_32(abfd,
	      in->x_file.x_n.x_offset,
	      (bfd_byte *) ext->x_file.x_n.x_offset);
    }
    else {
#if FILNMLEN != E_FILNMLEN
      -> Error, we need to cope with truncating or extending FILNMLEN!;
#else
      memcpy (ext->x_file.x_fname, in->x_file.x_fname, FILNMLEN);
#endif
    }
    return AUXESZ;


  case C_STAT:
#ifdef C_LEAFSTAT
  case C_LEAFSTAT:
#endif
  case C_HIDDEN:
    if (type == T_NULL) {
      PUT_SCN_SCNLEN(abfd, in->x_scn.x_scnlen, ext);
      PUT_SCN_NRELOC(abfd, in->x_scn.x_nreloc, ext);
      PUT_SCN_NLINNO(abfd, in->x_scn.x_nlinno, ext);
      bfd_h_put_32 (abfd, in->x_scn.x_checksum,
		    (bfd_byte *) ext->x_scn.x_checksum);
      bfd_h_put_16 (abfd, in->x_scn.x_associated,
		    (bfd_byte *) ext->x_scn.x_associated);
      bfd_h_put_8 (abfd, in->x_scn.x_comdat,
		   (bfd_byte *) ext->x_scn.x_comdat);
      return AUXESZ;
    }
    break;
  case C_NT_WEAK:
    bfd_h_put_32(abfd, in->x_sym.x_tagndx.l, (bfd_byte *) ext->x_sym.x_tagndx);
    /* Check for IMAGE_WEAK_EXTERN_SEARCH_ALIAS */
    BFD_ASSERT (in->x_sym.x_misc.x_fsize == 3);
    bfd_h_put_32 (abfd, in->x_sym.x_misc.x_fsize,
	     (bfd_byte *)  ext->x_sym.x_misc.x_fsize);
    return AUXESZ;
    
  }

  bfd_h_put_32(abfd, in->x_sym.x_tagndx.l, (bfd_byte *) ext->x_sym.x_tagndx);
#ifndef NO_TVNDX
  bfd_h_put_16(abfd, in->x_sym.x_tvndx , (bfd_byte *) ext->x_sym.x_tvndx);
#endif

  if (class == C_BLOCK || class == C_FCN || ISFCN (type) || ISTAG (class))
    {
      PUT_FCN_LNNOPTR(abfd,  in->x_sym.x_fcnary.x_fcn.x_lnnoptr, ext);
      PUT_FCN_ENDNDX(abfd,  in->x_sym.x_fcnary.x_fcn.x_endndx.l, ext);
    }
  else
    {
#if DIMNUM != E_DIMNUM
 #error we need to cope with truncating or extending DIMNUM
#endif
      bfd_h_put_16 (abfd, in->x_sym.x_fcnary.x_ary.x_dimen[0],
		    (bfd_byte *) ext->x_sym.x_fcnary.x_ary.x_dimen[0]);
      bfd_h_put_16 (abfd, in->x_sym.x_fcnary.x_ary.x_dimen[1],
		    (bfd_byte *) ext->x_sym.x_fcnary.x_ary.x_dimen[1]);
      bfd_h_put_16 (abfd, in->x_sym.x_fcnary.x_ary.x_dimen[2],
		    (bfd_byte *) ext->x_sym.x_fcnary.x_ary.x_dimen[2]);
      bfd_h_put_16 (abfd, in->x_sym.x_fcnary.x_ary.x_dimen[3],
		    (bfd_byte *) ext->x_sym.x_fcnary.x_ary.x_dimen[3]);
    }

  if (ISFCN (type))
    bfd_h_put_32 (abfd, in->x_sym.x_misc.x_fsize,
	     (bfd_byte *)  ext->x_sym.x_misc.x_fsize);
  else
    {
      PUT_LNSZ_LNNO (abfd, in->x_sym.x_misc.x_lnsz.x_lnno, ext);
      PUT_LNSZ_SIZE (abfd, in->x_sym.x_misc.x_lnsz.x_size, ext);
    }

  return AUXESZ;
}

void
pei_swap_lineno_in (abfd, ext1, in1)
     bfd            *abfd;
     PTR ext1;
     PTR in1;
{
  LINENO *ext = (LINENO *)ext1;
  struct internal_lineno      *in = (struct internal_lineno *)in1;

  in->l_addr.l_symndx = bfd_h_get_32(abfd, (bfd_byte *) ext->l_addr.l_symndx);
  in->l_lnno = GET_LINENO_LNNO(abfd, ext);
}

unsigned int
pei_swap_lineno_out (abfd, inp, outp)
     bfd       *abfd;
     PTR	inp;
     PTR	outp;
{
  struct internal_lineno *in = (struct internal_lineno *)inp;
  struct external_lineno *ext = (struct external_lineno *)outp;
  bfd_h_put_32(abfd, in->l_addr.l_symndx, (bfd_byte *)
	  ext->l_addr.l_symndx);

  PUT_LINENO_LNNO (abfd, in->l_lnno, ext);
  return LINESZ;
}

void
pei_swap_aouthdr_in (abfd, aouthdr_ext1, aouthdr_int1)
     bfd            *abfd;
     PTR aouthdr_ext1;
     PTR aouthdr_int1;
{
  struct internal_extra_pe_aouthdr *a;
  PEAOUTHDR *src = (PEAOUTHDR *)(aouthdr_ext1);
  AOUTHDR        *aouthdr_ext = (AOUTHDR *) aouthdr_ext1;
  struct internal_aouthdr *aouthdr_int = (struct internal_aouthdr *)aouthdr_int1;

  aouthdr_int->magic = bfd_h_get_16(abfd, (bfd_byte *) aouthdr_ext->magic);
  aouthdr_int->vstamp = bfd_h_get_16(abfd, (bfd_byte *) aouthdr_ext->vstamp);
  aouthdr_int->tsize =
    GET_AOUTHDR_TSIZE (abfd, (bfd_byte *) aouthdr_ext->tsize);
  aouthdr_int->dsize =
    GET_AOUTHDR_DSIZE (abfd, (bfd_byte *) aouthdr_ext->dsize);
  aouthdr_int->bsize =
    GET_AOUTHDR_BSIZE (abfd, (bfd_byte *) aouthdr_ext->bsize);
  aouthdr_int->entry =
    GET_AOUTHDR_ENTRY (abfd, (bfd_byte *) aouthdr_ext->entry);
  aouthdr_int->text_start =
    GET_AOUTHDR_TEXT_START (abfd, (bfd_byte *) aouthdr_ext->text_start);
  aouthdr_int->data_start =
    GET_AOUTHDR_DATA_START (abfd, (bfd_byte *) aouthdr_ext->data_start);

  a = &aouthdr_int->pe;
  a->ImageBase = bfd_h_get_32 (abfd, (unsigned char *)src->ImageBase);
  a->SectionAlignment = bfd_h_get_32 (abfd, (unsigned char *)src->SectionAlignment);
  a->FileAlignment = bfd_h_get_32 (abfd, (unsigned char *)src->FileAlignment);
  a->MajorOperatingSystemVersion = 
    bfd_h_get_16 (abfd, (unsigned char *)src->MajorOperatingSystemVersion);
  a->MinorOperatingSystemVersion = 
    bfd_h_get_16 (abfd, (unsigned char *)src->MinorOperatingSystemVersion);
  a->MajorImageVersion = bfd_h_get_16 (abfd, (unsigned char *)src->MajorImageVersion);
  a->MinorImageVersion = bfd_h_get_16 (abfd, (unsigned char *)src->MinorImageVersion);
  a->MajorSubsystemVersion = bfd_h_get_16 (abfd, (unsigned char *)src->MajorSubsystemVersion);
  a->MinorSubsystemVersion = bfd_h_get_16 (abfd, (unsigned char *)src->MinorSubsystemVersion);
  a->Reserved1 = bfd_h_get_32 (abfd, (unsigned char *)src->Reserved1);
  a->SizeOfImage = bfd_h_get_32 (abfd, (unsigned char *)src->SizeOfImage);
  a->SizeOfHeaders = bfd_h_get_32 (abfd, (unsigned char *)src->SizeOfHeaders);
  a->CheckSum = bfd_h_get_32 (abfd, (unsigned char *)src->CheckSum);
  a->Subsystem = bfd_h_get_16 (abfd, (unsigned char *)src->Subsystem);
  a->DllCharacteristics = bfd_h_get_16 (abfd, (unsigned char *)src->DllCharacteristics);
  a->SizeOfStackReserve = bfd_h_get_32 (abfd, (unsigned char *)src->SizeOfStackReserve);
  a->SizeOfStackCommit = bfd_h_get_32 (abfd, (unsigned char *)src->SizeOfStackCommit);
  a->SizeOfHeapReserve = bfd_h_get_32 (abfd, (unsigned char *)src->SizeOfHeapReserve);
  a->SizeOfHeapCommit = bfd_h_get_32 (abfd, (unsigned char *)src->SizeOfHeapCommit);
  a->LoaderFlags = bfd_h_get_32 (abfd, (unsigned char *)src->LoaderFlags);
  a->NumberOfRvaAndSizes = bfd_h_get_32 (abfd, (unsigned char *)src->NumberOfRvaAndSizes);

  /* Believe the count that's in the file. */
  {
    int idx;
    for (idx=0; idx < a->NumberOfRvaAndSizes; idx++)
      {
	a->DataDirectory[idx].VirtualAddress =
	  bfd_h_get_32 (abfd, (unsigned char *)src->DataDirectory[idx][0]);
	a->DataDirectory[idx].Size =
	  bfd_h_get_32 (abfd, (unsigned char *)src->DataDirectory[idx][1]);
      }
  }

#if 0
  /* these don't really have a lot of meaning on input, but for the record... */
  /* All NT platforms do use them.  These would want to be added to the
     internal image header if they were to actually be used.  However, there
     doesn't seem to be any use for them */
  first_thunk_address = a->DataDirectory[12].VirtualAddress ;
  thunk_size = a->DataDirectory[12].Size;
  import_table_size = a->DataDirectory[1].Size;
#endif

}

/* a support function for below */
static void add_data_entry
  PARAMS ((bfd *, struct internal_extra_pe_aouthdr *, int, char *, bfd_vma));
static void add_data_entry (abfd, aout, idx, name, base)
     bfd *abfd;
     struct internal_extra_pe_aouthdr *aout;
     int idx;
     char *name;
     bfd_vma base;
{
  asection *sec = bfd_get_section_by_name (abfd, name);
  /* NOTUSED: base */

  /* add import directory information if it exists */
  if (sec != NULL)
    {
      aout->DataDirectory[idx].VirtualAddress = sec->vma;
      aout->DataDirectory[idx].Size = pei_section_data (abfd, sec)->virt_size;
      sec->flags |= SEC_DATA;
    }
}

unsigned int
pei_swap_aouthdr_out (abfd, in, out)
     bfd       *abfd;
     PTR	in;
     PTR	out;
{
  struct internal_aouthdr *aouthdr_in = (struct internal_aouthdr *)in;
  struct internal_extra_pe_aouthdr *extra = &pe_data (abfd)->pe_opthdr;
  PEAOUTHDR *aouthdr_out = (PEAOUTHDR *)out;

  bfd_vma sa = extra->SectionAlignment;
  bfd_vma fa = extra->FileAlignment;

#define FA(x)  (((x) + fa -1 ) & (- fa))
#define SA(x)  (((x) + sa -1 ) & (- sa))

  /* We like to have the sizes aligned */

  aouthdr_in->bsize = FA (aouthdr_in->bsize);


  extra->NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;

  /* first null out all data directory entries .. */
  memset (extra->DataDirectory, sizeof (extra->DataDirectory), 0);

  add_data_entry (abfd, extra, 0, ".edata", 0);

  /* Don't--
     add_data_entry (abfd, extra, 1, ".idata$2", 0);
     add_data_entry (abfd, extra, 12, ".idata$5", 0); 
     because it's done in bfd_coff_final_link where all the information
     needed is available. */

  add_data_entry (abfd, extra, 2, ".rsrc" ,0);

  add_data_entry (abfd, extra, 3, ".pdata" ,0);

  /* for some reason, the virtual size (which is what's set by
     add_data_entry) for .reloc is not the same as the size recorded
     in this slot by MSVC; it doesn't seem to cause problems (so far),
     but since it's the best we've got, use it.  It does do the right
     thing for .pdata. */
  if (pe_data (abfd)->has_reloc_section)
      add_data_entry (abfd, extra, 5, ".reloc", 0);

  {
    asection *sec;
    bfd_vma dsize= 0;
    bfd_vma isize = SA(abfd->sections->filepos);
    bfd_vma tsize= 0;

    for (sec = abfd->sections; sec; sec = sec->next)
      {
	int rounded = FA(sec->_raw_size);

	if (sec->flags & SEC_DATA) 
	  dsize += rounded;
	if (sec->flags & SEC_CODE)
	  tsize += rounded;
	/* The image size is the total VIRTUAL size (which is what is in
	   the virt_size field).  Files have been seen (from MSVC 5.0 link.exe)
	   where the file size of the .data segment is quite small compared
	   to the virtual size.  Without this fix, strip munges the file. */
	isize += SA(FA(pei_section_data (abfd, sec)->virt_size));
      }

    aouthdr_in->dsize = dsize;
    aouthdr_in->tsize = tsize;
    extra->SizeOfImage = isize;
  }

  extra->SizeOfHeaders = abfd->sections->filepos;
  bfd_h_put_16(abfd, aouthdr_in->magic, (bfd_byte *) aouthdr_out->standard.magic);

#define LINKER_VERSION 256 /* That is, 2.56 */

  /* this piece of magic sets the "linker version" field to LINKER_VERSION */
  bfd_h_put_16(abfd, LINKER_VERSION/100  + (LINKER_VERSION % 100) * 256, (bfd_byte *) aouthdr_out->standard.vstamp);

  PUT_AOUTHDR_TSIZE (abfd, aouthdr_in->tsize, (bfd_byte *) aouthdr_out->standard.tsize);
  PUT_AOUTHDR_DSIZE (abfd, aouthdr_in->dsize, (bfd_byte *) aouthdr_out->standard.dsize);
  PUT_AOUTHDR_BSIZE (abfd, aouthdr_in->bsize, (bfd_byte *) aouthdr_out->standard.bsize);
  PUT_AOUTHDR_ENTRY (abfd, aouthdr_in->entry, (bfd_byte *) aouthdr_out->standard.entry);
  PUT_AOUTHDR_TEXT_START (abfd, aouthdr_in->text_start,
			  (bfd_byte *) aouthdr_out->standard.text_start);

  PUT_AOUTHDR_DATA_START (abfd, aouthdr_in->data_start,
			  (bfd_byte *) aouthdr_out->standard.data_start);


  bfd_h_put_32 (abfd, extra->ImageBase, 
		(bfd_byte *) aouthdr_out->ImageBase);
  bfd_h_put_32 (abfd, extra->SectionAlignment,
		(bfd_byte *) aouthdr_out->SectionAlignment);
  bfd_h_put_32 (abfd, extra->FileAlignment,
		(bfd_byte *) aouthdr_out->FileAlignment);
  bfd_h_put_16 (abfd, extra->MajorOperatingSystemVersion,
		(bfd_byte *) aouthdr_out->MajorOperatingSystemVersion);
  bfd_h_put_16 (abfd, extra->MinorOperatingSystemVersion,
		(bfd_byte *) aouthdr_out->MinorOperatingSystemVersion);
  bfd_h_put_16 (abfd, extra->MajorImageVersion,
		(bfd_byte *) aouthdr_out->MajorImageVersion);
  bfd_h_put_16 (abfd, extra->MinorImageVersion,
		(bfd_byte *) aouthdr_out->MinorImageVersion);
  bfd_h_put_16 (abfd, extra->MajorSubsystemVersion,
		(bfd_byte *) aouthdr_out->MajorSubsystemVersion);
  bfd_h_put_16 (abfd, extra->MinorSubsystemVersion,
		(bfd_byte *) aouthdr_out->MinorSubsystemVersion);
  bfd_h_put_32 (abfd, extra->Reserved1,
		(bfd_byte *) aouthdr_out->Reserved1);
  bfd_h_put_32 (abfd, extra->SizeOfImage,
		(bfd_byte *) aouthdr_out->SizeOfImage);
  bfd_h_put_32 (abfd, extra->SizeOfHeaders,
		(bfd_byte *) aouthdr_out->SizeOfHeaders);
  bfd_h_put_32 (abfd, extra->CheckSum,
		(bfd_byte *) aouthdr_out->CheckSum);
  bfd_h_put_16 (abfd, extra->Subsystem,
		(bfd_byte *) aouthdr_out->Subsystem);
  bfd_h_put_16 (abfd, extra->DllCharacteristics,
		(bfd_byte *) aouthdr_out->DllCharacteristics);
  bfd_h_put_32 (abfd, extra->SizeOfStackReserve,
		(bfd_byte *) aouthdr_out->SizeOfStackReserve);
  bfd_h_put_32 (abfd, extra->SizeOfStackCommit,
		(bfd_byte *) aouthdr_out->SizeOfStackCommit);
  bfd_h_put_32 (abfd, extra->SizeOfHeapReserve,
		(bfd_byte *) aouthdr_out->SizeOfHeapReserve);
  bfd_h_put_32 (abfd, extra->SizeOfHeapCommit,
		(bfd_byte *) aouthdr_out->SizeOfHeapCommit);
  bfd_h_put_32 (abfd, extra->LoaderFlags,
		(bfd_byte *) aouthdr_out->LoaderFlags);
  bfd_h_put_32 (abfd, extra->NumberOfRvaAndSizes,
		(bfd_byte *) aouthdr_out->NumberOfRvaAndSizes);
  {
    int idx;
    for (idx=0; idx < 16; idx++)
      {
	bfd_h_put_32 (abfd, extra->DataDirectory[idx].VirtualAddress,
		      (bfd_byte *) aouthdr_out->DataDirectory[idx][0]);
	bfd_h_put_32 (abfd, extra->DataDirectory[idx].Size,
		      (bfd_byte *) aouthdr_out->DataDirectory[idx][1]);
      }
  }

  return AOUTSZ;
}

unsigned int
pei_only_swap_filehdr_out (abfd, in, out)
     bfd       *abfd;
     PTR	in;
     PTR	out;
{
  int idx;
  struct internal_filehdr *filehdr_in = (struct internal_filehdr *)in;
  struct external_PEI_filehdr *filehdr_out = (struct external_PEI_filehdr *)out;

  if (pe_data (abfd)->has_reloc_section)
    filehdr_in->f_flags &= ~F_RELFLG;

  if (pe_data (abfd)->dll)
    filehdr_in->f_flags |= F_DLL;

  filehdr_in->pe.e_magic    = DOSMAGIC;
  filehdr_in->pe.e_cblp     = 0x90;
  filehdr_in->pe.e_cp       = 0x3;
  filehdr_in->pe.e_crlc     = 0x0;
  filehdr_in->pe.e_cparhdr  = 0x4;
  filehdr_in->pe.e_minalloc = 0x0;
  filehdr_in->pe.e_maxalloc = 0xffff;
  filehdr_in->pe.e_ss       = 0x0;
  filehdr_in->pe.e_sp       = 0xb8;
  filehdr_in->pe.e_csum     = 0x0;
  filehdr_in->pe.e_ip       = 0x0;
  filehdr_in->pe.e_cs       = 0x0;
  filehdr_in->pe.e_lfarlc   = 0x40;
  filehdr_in->pe.e_ovno     = 0x0;

  for (idx=0; idx < 4; idx++)
    filehdr_in->pe.e_res[idx] = 0x0;

  filehdr_in->pe.e_oemid   = 0x0;
  filehdr_in->pe.e_oeminfo = 0x0;

  for (idx=0; idx < 10; idx++)
    filehdr_in->pe.e_res2[idx] = 0x0;

  filehdr_in->pe.e_lfanew = 0x80;

  /* this next collection of data are mostly just characters.  It appears
     to be constant within the headers put on NT exes */
  filehdr_in->pe.dos_message[0]  = 0x0eba1f0e;
  filehdr_in->pe.dos_message[1]  = 0xcd09b400;
  filehdr_in->pe.dos_message[2]  = 0x4c01b821;
  filehdr_in->pe.dos_message[3]  = 0x685421cd;
  filehdr_in->pe.dos_message[4]  = 0x70207369;
  filehdr_in->pe.dos_message[5]  = 0x72676f72;
  filehdr_in->pe.dos_message[6]  = 0x63206d61;
  filehdr_in->pe.dos_message[7]  = 0x6f6e6e61;
  filehdr_in->pe.dos_message[8]  = 0x65622074;
  filehdr_in->pe.dos_message[9]  = 0x6e757220;
  filehdr_in->pe.dos_message[10] = 0x206e6920;
  filehdr_in->pe.dos_message[11] = 0x20534f44;
  filehdr_in->pe.dos_message[12] = 0x65646f6d;
  filehdr_in->pe.dos_message[13] = 0x0a0d0d2e;
  filehdr_in->pe.dos_message[14] = 0x24;
  filehdr_in->pe.dos_message[15] = 0x0;
  filehdr_in->pe.nt_signature = NT_SIGNATURE;



  bfd_h_put_16(abfd, filehdr_in->f_magic, (bfd_byte *) filehdr_out->f_magic);
  bfd_h_put_16(abfd, filehdr_in->f_nscns, (bfd_byte *) filehdr_out->f_nscns);

  bfd_h_put_32(abfd, time (0), (bfd_byte *) filehdr_out->f_timdat);
  PUT_FILEHDR_SYMPTR (abfd, (bfd_vma) filehdr_in->f_symptr,
		      (bfd_byte *) filehdr_out->f_symptr);
  bfd_h_put_32(abfd, filehdr_in->f_nsyms, (bfd_byte *) filehdr_out->f_nsyms);
  bfd_h_put_16(abfd, filehdr_in->f_opthdr, (bfd_byte *) filehdr_out->f_opthdr);
  bfd_h_put_16(abfd, filehdr_in->f_flags, (bfd_byte *) filehdr_out->f_flags);

  /* put in extra dos header stuff.  This data remains essentially
     constant, it just has to be tacked on to the beginning of all exes 
     for NT */
  bfd_h_put_16(abfd, filehdr_in->pe.e_magic, (bfd_byte *) filehdr_out->e_magic);
  bfd_h_put_16(abfd, filehdr_in->pe.e_cblp, (bfd_byte *) filehdr_out->e_cblp);
  bfd_h_put_16(abfd, filehdr_in->pe.e_cp, (bfd_byte *) filehdr_out->e_cp);
  bfd_h_put_16(abfd, filehdr_in->pe.e_crlc, (bfd_byte *) filehdr_out->e_crlc);
  bfd_h_put_16(abfd, filehdr_in->pe.e_cparhdr, 
	       (bfd_byte *) filehdr_out->e_cparhdr);
  bfd_h_put_16(abfd, filehdr_in->pe.e_minalloc, 
	       (bfd_byte *) filehdr_out->e_minalloc);
  bfd_h_put_16(abfd, filehdr_in->pe.e_maxalloc, 
	       (bfd_byte *) filehdr_out->e_maxalloc);
  bfd_h_put_16(abfd, filehdr_in->pe.e_ss, (bfd_byte *) filehdr_out->e_ss);
  bfd_h_put_16(abfd, filehdr_in->pe.e_sp, (bfd_byte *) filehdr_out->e_sp);
  bfd_h_put_16(abfd, filehdr_in->pe.e_csum, (bfd_byte *) filehdr_out->e_csum);
  bfd_h_put_16(abfd, filehdr_in->pe.e_ip, (bfd_byte *) filehdr_out->e_ip);
  bfd_h_put_16(abfd, filehdr_in->pe.e_cs, (bfd_byte *) filehdr_out->e_cs);
  bfd_h_put_16(abfd, filehdr_in->pe.e_lfarlc, (bfd_byte *) filehdr_out->e_lfarlc);
  bfd_h_put_16(abfd, filehdr_in->pe.e_ovno, (bfd_byte *) filehdr_out->e_ovno);
  {
    int idx;
    for (idx=0; idx < 4; idx++)
      bfd_h_put_16(abfd, filehdr_in->pe.e_res[idx], 
		   (bfd_byte *) filehdr_out->e_res[idx]);
  }
  bfd_h_put_16(abfd, filehdr_in->pe.e_oemid, (bfd_byte *) filehdr_out->e_oemid);
  bfd_h_put_16(abfd, filehdr_in->pe.e_oeminfo,
	       (bfd_byte *) filehdr_out->e_oeminfo);
  {
    int idx;
    for (idx=0; idx < 10; idx++)
      bfd_h_put_16(abfd, filehdr_in->pe.e_res2[idx],
		   (bfd_byte *) filehdr_out->e_res2[idx]);
  }
  bfd_h_put_32(abfd, filehdr_in->pe.e_lfanew, (bfd_byte *) filehdr_out->e_lfanew);

  {
    int idx;
    for (idx=0; idx < 16; idx++)
      bfd_h_put_32(abfd, filehdr_in->pe.dos_message[idx],
		   (bfd_byte *) filehdr_out->dos_message[idx]);
  }

  /* also put in the NT signature */
  bfd_h_put_32(abfd, filehdr_in->pe.nt_signature, 
	       (bfd_byte *) filehdr_out->nt_signature);




  return FILHSZ;
}

unsigned int
pe_only_swap_filehdr_out (abfd, in, out)
     bfd       *abfd;
     PTR	in;
     PTR	out;
{
  struct internal_filehdr *filehdr_in = (struct internal_filehdr *)in;
  FILHDR *filehdr_out = (FILHDR *)out;

  bfd_h_put_16(abfd, filehdr_in->f_magic, (bfd_byte *) filehdr_out->f_magic);
  bfd_h_put_16(abfd, filehdr_in->f_nscns, (bfd_byte *) filehdr_out->f_nscns);
  bfd_h_put_32(abfd, filehdr_in->f_timdat, (bfd_byte *) filehdr_out->f_timdat);
  PUT_FILEHDR_SYMPTR (abfd, (bfd_vma) filehdr_in->f_symptr,
		      (bfd_byte *) filehdr_out->f_symptr);
  bfd_h_put_32(abfd, filehdr_in->f_nsyms, (bfd_byte *) filehdr_out->f_nsyms);
  bfd_h_put_16(abfd, filehdr_in->f_opthdr, (bfd_byte *) filehdr_out->f_opthdr);
  bfd_h_put_16(abfd, filehdr_in->f_flags, (bfd_byte *) filehdr_out->f_flags);

  return FILHSZ;
}


unsigned int
pei_swap_scnhdr_out (abfd, in, out)
     bfd       *abfd;
     PTR	in;
     PTR	out;
{
  struct internal_scnhdr *scnhdr_int = (struct internal_scnhdr *)in;
  SCNHDR *scnhdr_ext = (SCNHDR *)out;
  unsigned int ret = SCNHSZ;
  bfd_vma ps;
  bfd_vma ss;

  memcpy(scnhdr_ext->s_name, scnhdr_int->s_name, sizeof(scnhdr_int->s_name));

  PUT_SCNHDR_VADDR (abfd, 
		    (scnhdr_int->s_vaddr),
		    (bfd_byte *) scnhdr_ext->s_vaddr);

  /* NT wants the size data to be rounded up to the next NT_FILE_ALIGNMENT,
     but zero if it has no content (as in .bss, sometimes).  However, this
     applies only to executables, not objects. */

  if (scnhdr_int->s_flags & IMAGE_SCN_CNT_UNINITIALIZED_DATA
     && coff_data (abfd)->link_info
     && ! coff_data (abfd)->link_info->relocateable)
    {
      ps = scnhdr_int->s_size;
      ss = 0;
    }
  else
    {
      ps = scnhdr_int->s_paddr;
      ss = scnhdr_int->s_size;
    }

  PUT_SCNHDR_SIZE (abfd, ss,
		   (bfd_byte *) scnhdr_ext->s_size);


  /* s_paddr in PE is really the virtual size */
  PUT_SCNHDR_PADDR (abfd, ps, (bfd_byte *) scnhdr_ext->s_paddr);

  PUT_SCNHDR_SCNPTR (abfd, scnhdr_int->s_scnptr,
		     (bfd_byte *) scnhdr_ext->s_scnptr);
  PUT_SCNHDR_RELPTR (abfd, scnhdr_int->s_relptr,
		     (bfd_byte *) scnhdr_ext->s_relptr);
  PUT_SCNHDR_LNNOPTR (abfd, scnhdr_int->s_lnnoptr,
		      (bfd_byte *) scnhdr_ext->s_lnnoptr);

  /* Extra flags must be set when dealing with NT.  All sections should also
     have the IMAGE_SCN_MEM_READ (0x40000000) flag set.  In addition, the
     .text section must have IMAGE_SCN_MEM_EXECUTE (0x20000000) and the data
     sections (.idata, .data, .bss, .CRT) must have IMAGE_SCN_MEM_WRITE set
     (this is especially important when dealing with the .idata section since
     the addresses for routines from .dlls must be overwritten).  If .reloc
     section data is ever generated, we must add IMAGE_SCN_MEM_DISCARDABLE
     (0x02000000).  Also, the resource data should also be read and
     writable.  */

  /* FIXME: alignment is also encoded in this field, at least on ppc (krk) */
  /* FIXME: even worse, I don't see how to get the original alignment field*/
  /*        back...                                                        */

  {
    int flags = scnhdr_int->s_flags;
#if 0 /* See coffcode.h, where this is done based on input data */
      /* rather than a-priori knowledge of section names. */
    if (strcmp (scnhdr_int->s_name, ".data")  == 0 ||
	strcmp (scnhdr_int->s_name, ".CRT")   == 0 ||
	strcmp (scnhdr_int->s_name, ".rsrc")  == 0 ||
	strcmp (scnhdr_int->s_name, ".bss")   == 0)
      flags |= IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;
    else if (strcmp (scnhdr_int->s_name, ".text") == 0)
      flags |= IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE;
    else if (strcmp (scnhdr_int->s_name, ".reloc") == 0)
      flags = SEC_DATA| IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_DISCARDABLE;
    else if (strcmp (scnhdr_int->s_name, ".idata") == 0)
      flags = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE | SEC_DATA;     
    else if (strcmp (scnhdr_int->s_name, ".rdata") == 0
	     || strcmp (scnhdr_int->s_name, ".edata") == 0)
      flags =  IMAGE_SCN_MEM_READ | SEC_DATA;     
    else if (strcmp (scnhdr_int->s_name, ".pdata") == 0)
      flags = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_ALIGN_4BYTES |
			  IMAGE_SCN_MEM_READ ;
    /* Remember this field is a max of 8 chars, so the null is _not_ there
       for an 8 character name like ".reldata". (yep. Stupid bug) */
    else if (strncmp (scnhdr_int->s_name, ".reldata", strlen(".reldata")) == 0)
      flags =  IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_ALIGN_8BYTES |
	       IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE ;
    else if (strcmp (scnhdr_int->s_name, ".ydata") == 0)
      flags =  IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_ALIGN_8BYTES |
	       IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE ;
    else if (strncmp (scnhdr_int->s_name, ".drectve", strlen(".drectve")) == 0)
      flags =  IMAGE_SCN_LNK_INFO | IMAGE_SCN_LNK_REMOVE ;
#ifdef POWERPC_LE_PE
    else if (strncmp (scnhdr_int->s_name, ".stabstr", strlen(".stabstr")) == 0)
      {
	flags =  IMAGE_SCN_LNK_INFO;
      }
    else if (strcmp (scnhdr_int->s_name, ".stab") == 0)
      {
	flags =  IMAGE_SCN_LNK_INFO;
      }
#endif
#endif /* 0 */

    bfd_h_put_32(abfd, flags, (bfd_byte *) scnhdr_ext->s_flags);
  }

  if (coff_data (abfd)->link_info && 
     !coff_data (abfd)->link_info->relocateable &&
     !coff_data (abfd)->link_info->shared &&
     strcmp(scnhdr_int->s_name,".text") == 0)
      
    {
      /* By inference from looking at MS output, the 32 bit field which
	 is the combintion of the number_of_relocs and number_of_linenos
	 is used for the line number count in executables.  A 16-bit
	 field won't do for cc1.   The MS document says that the number
	 of relocs is zero for executables, but the 17-th bit has been
	 observed to be there.  Overflow is not an issue: a 4G-line
	 program will overflow a bunch of other fields long before this! */
      bfd_h_put_16(abfd, scnhdr_int->s_nlnno & 0xffff, 
			 (bfd_byte *) scnhdr_ext->s_nlnno);
      bfd_h_put_16(abfd, scnhdr_int->s_nlnno >> 16, 
			 (bfd_byte *) scnhdr_ext->s_nreloc);
    }
  else 
    {
      if (scnhdr_int->s_nlnno <= 0xffff)
	bfd_h_put_16(abfd, scnhdr_int->s_nlnno, (bfd_byte *) scnhdr_ext->s_nlnno);
      else
	{
	  (*_bfd_error_handler) ("%s: line number overflow: 0x%lx > 0xffff",
				 bfd_get_filename (abfd),
				 scnhdr_int->s_nlnno);
	  bfd_set_error (bfd_error_file_truncated);
	  bfd_h_put_16 (abfd, 0xffff, (bfd_byte *) scnhdr_ext->s_nlnno);
	  ret = 0;
	}
      if (scnhdr_int->s_nreloc <= 0xffff)
	bfd_h_put_16(abfd, scnhdr_int->s_nreloc, (bfd_byte *) scnhdr_ext->s_nreloc);
      else
	{
	  (*_bfd_error_handler) ("%s: reloc overflow: 0x%lx > 0xffff",
				 bfd_get_filename (abfd),
				 scnhdr_int->s_nreloc);
	  bfd_set_error (bfd_error_file_truncated);
	  bfd_h_put_16 (abfd, 0xffff, (bfd_byte *) scnhdr_ext->s_nreloc);
	  ret = 0;
	}
    }
  return ret;
}

#ifdef DYNAMIC_LINKING

void
pei_swap_dyn_out (abfd, src, dst)
     bfd *abfd;
     const coff_internal_dyn *src;
     coff_external_dyn *dst;
{
  bfd_h_put_32 (abfd, src->d_tag, dst->d_tag);
  bfd_h_put_32 (abfd, src->d_un.d_val, dst->d_un.d_val);
}

void
pei_swap_dyn_in (abfd, p, dst)
     bfd *abfd;
     const PTR p;
     coff_internal_dyn *dst;
{
  const coff_external_dyn *src = (const coff_external_dyn *) p;

  dst->d_tag = bfd_h_get_32 (abfd, src->d_tag);
  dst->d_un.d_val = bfd_h_get_32 (abfd, src->d_un.d_val);
}

/* Swap in a Verdef structure.  */

void
pei_swap_verdef_in (abfd, src, dst)
     bfd *abfd;
     const coff_external_verdef *src;
     coff_internal_verdef *dst;
{
  dst->vd_version = bfd_h_get_16 (abfd, src->vd_version);
  dst->vd_flags   = bfd_h_get_16 (abfd, src->vd_flags);
  dst->vd_ndx     = bfd_h_get_16 (abfd, src->vd_ndx);
  dst->vd_cnt     = bfd_h_get_16 (abfd, src->vd_cnt);
  dst->vd_hash    = bfd_h_get_32 (abfd, src->vd_hash);
  dst->vd_aux     = bfd_h_get_32 (abfd, src->vd_aux);
  dst->vd_next    = bfd_h_get_32 (abfd, src->vd_next);
}

/* Swap out a Verdef structure.  */

void
pei_swap_verdef_out (abfd, src, dst)
     bfd *abfd;
     const coff_internal_verdef *src;
     coff_external_verdef *dst;
{
  bfd_h_put_16 (abfd, src->vd_version, dst->vd_version);
  bfd_h_put_16 (abfd, src->vd_flags, dst->vd_flags);
  bfd_h_put_16 (abfd, src->vd_ndx, dst->vd_ndx);
  bfd_h_put_16 (abfd, src->vd_cnt, dst->vd_cnt);
  bfd_h_put_32 (abfd, src->vd_hash, dst->vd_hash);
  bfd_h_put_32 (abfd, src->vd_aux, dst->vd_aux);
  bfd_h_put_32 (abfd, src->vd_next, dst->vd_next);
}

/* Swap in a Verdaux structure.  */

void
pei_swap_verdaux_in (abfd, src, dst)
     bfd *abfd;
     const coff_external_verdaux *src;
     coff_internal_verdaux *dst;
{
  dst->vda_name = bfd_h_get_32 (abfd, src->vda_name);
  dst->vda_next = bfd_h_get_32 (abfd, src->vda_next);
}

/* Swap out a Verdaux structure.  */

void
pei_swap_verdaux_out (abfd, src, dst)
     bfd *abfd;
     const coff_internal_verdaux *src;
     coff_external_verdaux *dst;
{
  bfd_h_put_32 (abfd, src->vda_name, dst->vda_name);
  bfd_h_put_32 (abfd, src->vda_next, dst->vda_next);
}

/* Swap in a Verneed structure.  */

void
pei_swap_verneed_in (abfd, src, dst)
     bfd *abfd;
     const coff_external_verneed *src;
     coff_internal_verneed *dst;
{
  dst->vn_version = bfd_h_get_16 (abfd, src->vn_version);
  dst->vn_cnt     = bfd_h_get_16 (abfd, src->vn_cnt);
  dst->vn_file    = bfd_h_get_32 (abfd, src->vn_file);
  dst->vn_aux     = bfd_h_get_32 (abfd, src->vn_aux);
  dst->vn_next    = bfd_h_get_32 (abfd, src->vn_next);
}

/* Swap out a Verneed structure.  */

void
pei_swap_verneed_out (abfd, src, dst)
     bfd *abfd;
     const coff_internal_verneed *src;
     coff_external_verneed *dst;
{
  bfd_h_put_16 (abfd, src->vn_version, dst->vn_version);
  bfd_h_put_16 (abfd, src->vn_cnt, dst->vn_cnt);
  bfd_h_put_32 (abfd, src->vn_file, dst->vn_file);
  bfd_h_put_32 (abfd, src->vn_aux, dst->vn_aux);
  bfd_h_put_32 (abfd, src->vn_next, dst->vn_next);
}

/* Swap in a Vernaux structure.  */

void
pei_swap_vernaux_in (abfd, src, dst)
     bfd *abfd;
     const coff_external_vernaux *src;
     coff_internal_vernaux *dst;
{
  dst->vna_hash  = bfd_h_get_32 (abfd, src->vna_hash);
  dst->vna_flags = bfd_h_get_16 (abfd, src->vna_flags);
  dst->vna_other = bfd_h_get_16 (abfd, src->vna_other);
  dst->vna_name  = bfd_h_get_32 (abfd, src->vna_name);
  dst->vna_next  = bfd_h_get_32 (abfd, src->vna_next);
}

/* Swap out a Vernaux structure.  */

void
pei_swap_vernaux_out (abfd, src, dst)
     bfd *abfd;
     const coff_internal_vernaux *src;
     coff_external_vernaux *dst;
{
  bfd_h_put_32 (abfd, src->vna_hash, dst->vna_hash);
  bfd_h_put_16 (abfd, src->vna_flags, dst->vna_flags);
  bfd_h_put_16 (abfd, src->vna_other, dst->vna_other);
  bfd_h_put_32 (abfd, src->vna_name, dst->vna_name);
  bfd_h_put_32 (abfd, src->vna_next, dst->vna_next);
}

/* Swap in a Versym structure.  */

void
pei_swap_versym_in (abfd, src, dst)
     bfd *abfd;
     const coff_external_versym *src;
     coff_internal_versym *dst;
{
  dst->vs_vers = bfd_h_get_16 (abfd, src->vs_vers);
}

/* Swap out a Versym structure.  */

void
pei_swap_versym_out (abfd, src, dst)
     bfd *abfd;
     const coff_internal_versym *src;
     coff_external_versym *dst;
{
  bfd_h_put_16 (abfd, src->vs_vers, dst->vs_vers);
}
#endif /* DYNAMIC_LINKING */

static char * dir_names[IMAGE_NUMBEROF_DIRECTORY_ENTRIES] = 
{
  "Export Directory [.edata (or where ever we found it)]",
  "Import Directory [parts of .idata]",
  "Resource Directory [.rsrc]",
  "Exception Directory [.pdata]",
  "Security Directory",
  "Base Relocation Directory [.reloc]",
  "Debug Directory",
  "Description Directory",
  "Special Directory",
  "Thread Storage Directory [.tls]",
  "Load Configuration Directory",
  "Bound Import Directory",
  "Import Address Table Directory",
  "Reserved",
  "Reserved",
  "Reserved"
};

/**********************************************************************/
struct addrinfo {
    asection *section;
    bfd_vma needed;
};

#ifdef POWERPC_LE_PE
/* The code for the PPC really falls in the "architecture dependent"
   category.  However, it's not clear that anyone will ever care, so
   we're ignoring the issue for now; if/when PPC matters, some of this
   may need to go into peicode.h, or arguments passed to enable the PPC-
   specific code. */
#endif

static void addr_in_range_p PARAMS ((bfd * abfd, asection * sect, PTR obj));
static void addr_in_range_p (abfd, sect, obj)
    bfd *abfd;
    asection *sect;
    PTR obj;
{
   struct addrinfo *addrinfo = (struct addrinfo *)obj;

   if (addrinfo->needed >= sect->vma &&
       addrinfo->needed < sect->vma + bfd_section_size(abfd,sect))
     {
	 addrinfo->section = sect;
     }
}

static boolean pe_print_idata PARAMS ((bfd *, PTR));
static boolean
pe_print_idata(abfd, vfile)
     bfd *abfd;
     PTR vfile;
{
  FILE *file = (FILE *) vfile;
  bfd_byte *data = 0;
  asection *section = bfd_get_section_by_name (abfd, ".idata");
  bfd_vma tableoffset = 0;

#ifdef POWERPC_LE_PE
  asection *rel_section = bfd_get_section_by_name (abfd, ".reldata");
#endif

  bfd_size_type datasize = 0;
  bfd_size_type i;
  bfd_size_type start, stop;
  int onaline = 20;

  pe_data_type *pe = pe_data (abfd);
  struct internal_extra_pe_aouthdr *extra = &pe->pe_opthdr;

  if (section == 0) 
    {
      if (extra->DataDirectory[1].VirtualAddress != 0) 
        {
	  struct addrinfo addrinfo;

/* ??? */
	  /* NT KERNEL32.DLL (at least) has its import table buried
	     in the middle of .text.  We'll figure it out and dump it
	     here. */
	  addrinfo.needed = extra->DataDirectory[1].VirtualAddress;
	  addrinfo.section = 0;
	  bfd_map_over_sections (abfd, addr_in_range_p, &addrinfo);
	  section = addrinfo.section;
	  if (addrinfo.section != 0)
	    {
	       fprintf(file,"\nThere is an import table in %s at 0x%lx\n",
		  section->name, (unsigned long)addrinfo.needed);
	       tableoffset=addrinfo.needed - section->vma;
	    }
	  else 
	    {
	       fprintf(file,"\nThere is an import table, but the section containing it could not be found\n");
               return true;
	    }
        }
	else
	    return true;
    }
  else
    fprintf(file,"\nThe import table is the .idata section\n");

#ifdef POWERPC_LE_PE
  if (rel_section != 0 && bfd_section_size (abfd, rel_section) != 0)
    {
      /* The toc address can be found by taking the starting address,
	 which on the PPC locates a function descriptor. The descriptor
	 consists of the function code starting address followed by the
	 address of the toc. The starting address we get from the bfd,
	 and the descriptor is supposed to be in the .reldata section. 
      */

      bfd_vma loadable_toc_address;
      bfd_vma toc_address;
      bfd_vma start_address;
      bfd_byte *data = 0;
      int offset;
      data = (bfd_byte *) bfd_malloc ((size_t) bfd_section_size (abfd, 
								 rel_section));
      if (data == NULL && bfd_section_size (abfd, rel_section) != 0)
	return false;

      datasize = bfd_section_size (abfd, rel_section);
  
      bfd_get_section_contents (abfd, 
				rel_section, 
				(PTR) data, 0, 
				bfd_section_size (abfd, rel_section));

      offset = abfd->start_address - rel_section->vma;

      start_address = bfd_get_32(abfd, data+offset);
      loadable_toc_address = bfd_get_32(abfd, data+offset+4);
      toc_address = loadable_toc_address - 32768;

      fprintf(file,
	      "\nFunction descriptor located at the start address: %04lx\n",
	      (unsigned long int) (abfd->start_address));
      fprintf (file,
	       "\tcode-base %08lx toc (loadable/actual) %08lx/%08lx\n", 
	       start_address, loadable_toc_address, toc_address);
    }
  else 
    {
      fprintf(file,
	      "\nNo reldata section! Function descriptor not decoded.\n");
    }
#endif

  fprintf(file,
	  "The Import Tables\n");
  fprintf(file,
	  " vma:    Hint    Time      Forward  DLL       First\n");
  fprintf(file,
	  "         Table   Stamp     Chain    Name      Thunk\n");

  if (bfd_section_size (abfd, section) == 0)
    return true;

  data = (bfd_byte *) bfd_malloc ((size_t) bfd_section_size (abfd, section));
  datasize = bfd_section_size (abfd, section);
  if (data == NULL && datasize != 0)
    return false;

  bfd_get_section_contents (abfd, 
			    section, 
			    (PTR) data, 0, 
			    bfd_section_size (abfd, section));

  start = 0;

  stop = bfd_section_size (abfd, section);

  for (i = start; i < stop; i += onaline)
    {
      bfd_vma hint_addr;
      bfd_vma time_stamp;
      bfd_vma forward_chain;
      bfd_vma dll_name;
      bfd_vma first_thunk;
      int idx;
      bfd_size_type j;
      char *dll;
      int adj = section->vma;

      fprintf (file,
	       " %04lx\t", 
	       (unsigned long int) (i + section->vma));
      
      if (i+20 > stop)
	{
	  /* check stuff */
	  ;
	}
      
      hint_addr = bfd_get_32(abfd, data+i+tableoffset);
      time_stamp = bfd_get_32(abfd, data+i+4+tableoffset);
      forward_chain = bfd_get_32(abfd, data+i+8+tableoffset);
      dll_name = bfd_get_32(abfd, data+i+12+tableoffset);
      first_thunk = bfd_get_32(abfd, data+i+16+tableoffset);
      
      fprintf(file, "%08lx %08lx %08lx %08lx %08lx\n",
	      hint_addr,
	      time_stamp,
	      forward_chain,
	      dll_name,
	      first_thunk);

      if (hint_addr ==0)
	{
	  break;
	}

      /* the image base is present in the section->vma */
      dll = (char *) data + dll_name - adj;
      fprintf(file, "\n\tDLL Name: %s\n", dll);

      /* 0x800000 is a wild guess; file size would be better ??? */
      if (hint_addr > 0x8000000) 
	{
	  abort();
	}

      fprintf(file, "\tvma:  Ordinal  Member-Name\n");

      idx = hint_addr - adj;

      for (j=0;j<stop;j+=4)
	{
	  int ordinal;
	  char *member_name;
	  bfd_vma member = bfd_get_32(abfd, data + idx + j);
	  if (member == 0)
	    break;
	  if ((member & 0x80000000) != 0)
	    {
	      member = ordinal = member & 0x7fffffff;
	      member_name = "(by ordinal)";
	    }
	  else
	    {
	      ordinal = bfd_get_16(abfd,
			       data + member - adj);
	      member_name = (char *) data + member - adj + 2;
	    }
	  fprintf(file, "\t%04lx\t %4d  %s\n",
		  member, ordinal, member_name);
	}

      if (hint_addr != first_thunk) 
	{
	  int differ = 0;
	  int idx2;

	  idx2 = first_thunk - adj;

	  for (j=0;j<stop;j+=4)
	    {
	      int ordinal;
	      char *member_name;
	      bfd_vma hint_member = bfd_get_32(abfd, data + idx + j);
	      bfd_vma iat_member = bfd_get_32(abfd, data + idx2 + j);
	      if (hint_member != iat_member)
		{
		  if (differ == 0)
		    {
		      fprintf(file, 
			      "\tThe Import Address Table (difference found)\n");
		      fprintf(file, "\tvma:  Ordinal  Slot# Member-Name\n");
		      differ = 1;
		    }
		  if (hint_member == 0)
		    {
		      fprintf(file,
			      "\t>>> Ran out of HINT members!\n");
		    }
		  else if (iat_member == 0)
		    {
		      fprintf(file,
			      "\t>>> Ran out of IAT members!\n");
		    }
		  else 
		    {
		      ordinal = bfd_get_16(abfd,
					   data + iat_member - adj);
		      member_name = (char *) data + iat_member - adj + 2;
		      fprintf(file, "\t%04lx\t %4d  %4ld  %s\n",
			      iat_member, ordinal, j/4, member_name);
		    }
		}
	      if (hint_member == 0 || iat_member == 0)
		break;
	    }
	  if (differ == 0)
	    {
	      fprintf(file,
		      "\tThe Import Address Table is identical\n");
	    }
	}

      fprintf(file, "\n");

    }

  free (data);

  return true;
}

static boolean pe_print_edata PARAMS ((bfd *, PTR));
static boolean
pe_print_edata (abfd, vfile)
     bfd *abfd;
     PTR vfile;
{
  FILE *file = (FILE *) vfile;
  bfd_byte *data = 0;
  bfd_byte *offsetdata = 0;
  asection *section = bfd_get_section_by_name (abfd, ".edata");

  bfd_size_type datasize = 0;
  int i;

  int adj;
  struct EDT_type 
    {
      long export_flags;             /* reserved - should be zero */
      long time_stamp;
      short major_ver;
      short minor_ver;
      bfd_vma name;                  /* rva - relative to image base */
      long base;                     /* ordinal base */
      long num_functions;        /* Number in the export address table */
      long num_names;            /* Number in the name pointer table */
      bfd_vma eat_addr;    /* rva to the export address table */
      bfd_vma npt_addr;        /* rva to the Export Name Pointer Table */
      bfd_vma ot_addr; /* rva to the Ordinal Table */
    } edt;

  bfd_vma tableoffset = 0;
  pe_data_type *pe = pe_data (abfd);
  struct internal_extra_pe_aouthdr *extra = &pe->pe_opthdr;

  if (section == 0) 
    {
      if (extra->DataDirectory[0].VirtualAddress != 0) 
        {
	  struct addrinfo addrinfo;

/* ??? */
	  /* NT NTDLL.DLL (at least) has its export table buried
	     in the middle of .text.  We'll figure it out and dump it
	     here. */
	  addrinfo.needed = extra->DataDirectory[0].VirtualAddress;
	  addrinfo.section = 0;
	  bfd_map_over_sections (abfd, addr_in_range_p, &addrinfo);
	  section = addrinfo.section;
	  if (addrinfo.section != 0)
	    {
	       fprintf(file,"\nThere is an export table in %s at 0x%lx\n",
		  section->name, (unsigned long)addrinfo.needed);
	       tableoffset=addrinfo.needed - section->vma;
	    }
	  else 
	    {
	       fprintf(file,"\nThere is an import table, but the section containing it could not be found\n");
               return true;
	    }
        }
	else
	    return true;
    }
  else
    fprintf(file,"\nThe import table is the .idata section\n");

  data = (bfd_byte *) bfd_malloc ((size_t) bfd_section_size (abfd, 
							     section));
  datasize = bfd_section_size (abfd, section);

  if (data == NULL && datasize != 0)
    return false;

  bfd_get_section_contents (abfd, 
			    section, 
			    (PTR) data, 0, 
			    bfd_section_size (abfd, section));

  offsetdata = data+ tableoffset;
  /* Go get Export Directory Table */
  edt.export_flags   = bfd_get_32(abfd, offsetdata+0); 
  edt.time_stamp     = bfd_get_32(abfd, offsetdata+4);
  edt.major_ver      = bfd_get_16(abfd, offsetdata+8);
  edt.minor_ver      = bfd_get_16(abfd, offsetdata+10);
  edt.name           = bfd_get_32(abfd, offsetdata+12);
  edt.base           = bfd_get_32(abfd, offsetdata+16);
  edt.num_functions  = bfd_get_32(abfd, offsetdata+20); 
  edt.num_names      = bfd_get_32(abfd, offsetdata+24); 
  edt.eat_addr       = bfd_get_32(abfd, offsetdata+28);
  edt.npt_addr       = bfd_get_32(abfd, offsetdata+32); 
  edt.ot_addr        = bfd_get_32(abfd, offsetdata+36);

  adj = section->vma;


  /* Dump the EDT first first */
  fprintf(file,
	  "\nThe Export Tables (interpreted .edata section contents)\n\n");

  fprintf(file,
	  "Export Flags \t\t\t%lx\n", (unsigned long) edt.export_flags);

  fprintf(file,
	  "Time/Date stamp \t\t%lx\n", (unsigned long) edt.time_stamp);

  fprintf(file,
	  "Major/Minor \t\t\t%d/%d\n", edt.major_ver, edt.minor_ver);

  fprintf (file,
	   "Name \t\t\t\t");
  fprintf_vma (file, edt.name);
  fprintf (file,
	   " %s\n", data + edt.name - adj);

  fprintf(file,
	  "Ordinal Base \t\t\t%ld\n", edt.base);

  fprintf(file,
	  "Number in:\n");

  fprintf(file,
	  "\tExport Address Table \t\t%lx\n",
	  (unsigned long) edt.num_functions);

  fprintf(file,
	  "\t[Name Pointer/Ordinal] Table\t%ld\n", edt.num_names);

  fprintf(file,
	  "Table Addresses\n");

  fprintf (file,
	   "\tExport Address Table \t\t");
  fprintf_vma (file, edt.eat_addr);
  fprintf (file, "\n");

  fprintf (file,
	  "\tName Pointer Table \t\t");
  fprintf_vma (file, edt.npt_addr);
  fprintf (file, "\n");

  fprintf (file,
	   "\tOrdinal Table \t\t\t");
  fprintf_vma (file, edt.ot_addr);
  fprintf (file, "\n");

  
  /* The next table to find si the Export Address Table. It's basically
     a list of pointers that either locate a function in this dll, or
     forward the call to another dll. Something like:
      typedef union 
      {
        long export_rva;
        long forwarder_rva;
      } export_address_table_entry;
  */

  fprintf(file,
	  "\nExport Address Table -- Ordinal Base %ld\n",
	  edt.base);

  for (i = 0; i < edt.num_functions; ++i)
    {
      bfd_vma eat_member = bfd_get_32(abfd, 
				      data + edt.eat_addr + (i*4) - adj);
      bfd_vma eat_actual = eat_member;
      bfd_vma edata_start = bfd_get_section_vma(abfd,section);
      bfd_vma edata_end = edata_start + bfd_section_size (abfd, section);


      if (eat_member == 0)
	continue;

      if (edata_start < eat_actual && eat_actual < edata_end) 
	{
	  /* this rva is to a name (forwarding function) in our section */
	  /* Should locate a function descriptor */
	  fprintf(file,
		  "\t[%4ld] +base[%4ld] %04lx %s -- %s\n", 
		  (long) i, (long) (i + edt.base), eat_member,
		  "Forwarder RVA", data + eat_member - adj);
	}
      else
	{
	  /* Should locate a function descriptor in the reldata section */
	  fprintf(file,
		  "\t[%4ld] +base[%4ld] %04lx %s\n", 
		  (long) i, (long) (i + edt.base), eat_member, "Export RVA");
	}
    }

  /* The Export Name Pointer Table is paired with the Export Ordinal Table */
  /* Dump them in parallel for clarity */
  fprintf(file,
	  "\n[Ordinal/Name Pointer] Table\n");

  for (i = 0; i < edt.num_names; ++i)
    {
      bfd_vma name_ptr = bfd_get_32(abfd, 
				    data + 
				    edt.npt_addr
				    + (i*4) - adj);
      
      char *name = (char *) data + name_ptr - adj;

      bfd_vma ord = bfd_get_16(abfd, 
				    data + 
				    edt.ot_addr
				    + (i*2) - adj);
      fprintf(file,
	      "\t[%4ld] %s\n", (long) ord, name);

    }

  free (data);

  return true;
}

static boolean pe_print_pdata PARAMS ((bfd *, PTR));
static boolean
pe_print_pdata (abfd, vfile)
     bfd  *abfd;
     PTR vfile;
{
  FILE *file = (FILE *) vfile;
  bfd_byte *data = 0;
  asection *section = bfd_get_section_by_name (abfd, ".pdata");
  bfd_size_type datasize = 0;
  bfd_size_type i;
  bfd_size_type start, stop;
  int onaline = 20;

  if (section == 0
      || coff_section_data(abfd, section) == 0
      || pei_section_data(abfd, section) == 0)
     return true;

  stop = pei_section_data (abfd, section)->virt_size;
  if ((stop % onaline) != 0)
    fprintf (file, "Warning, .pdata section size (%ld) is not a multiple of %d\n",
	     (long)stop, onaline);

  fprintf(file,
	  "\nThe Function Table (interpreted .pdata section contents)\n");
  fprintf(file,
	  " vma:\t\tBegin    End      EH       EH       PrologEnd  Exception\n");
  fprintf(file,
	  "     \t\tAddress  Address  Handler  Data     Address    Mask\n");

  if (bfd_section_size (abfd, section) == 0)
    return true;

  data = (bfd_byte *) bfd_malloc ((size_t) bfd_section_size (abfd, section));
  datasize = bfd_section_size (abfd, section);
  if (data == NULL && datasize != 0)
    return false;

  bfd_get_section_contents (abfd, 
			    section, 
			    (PTR) data, 0, 
			    bfd_section_size (abfd, section));

  start = 0;

  for (i = start; i < stop; i += onaline)
    {
      bfd_vma begin_addr;
      bfd_vma end_addr;
      bfd_vma eh_handler;
      bfd_vma eh_data;
      bfd_vma prolog_end_addr;
      int em_data;

      if (i+20 > stop)
	  break;
      
      begin_addr = bfd_get_32(abfd, data+i);
      end_addr = bfd_get_32(abfd, data+i+4);
      eh_handler = bfd_get_32(abfd, data+i+8);
      eh_data = bfd_get_32(abfd, data+i+12);
      prolog_end_addr = bfd_get_32(abfd, data+i+16);
      
      if (begin_addr == 0 && end_addr == 0 && eh_handler == 0
	  && eh_data == 0 && prolog_end_addr == 0)
	{
	  /* We are probably into the padding of the
	     section now */
	  break;
	}

      fprintf (file,
	       " %08lx\t", 
	       (unsigned long int) (i + section->vma));

      em_data = ((eh_handler&0x1)<<2) | (prolog_end_addr&0x3);
      eh_handler &= 0xfffffffc;
      prolog_end_addr &= 0xfffffffc;

      fprintf(file, "%08lx %08lx %08lx %08lx %08lx   %x",
	      begin_addr,
	      end_addr,
	      eh_handler,
	      eh_data,
	      prolog_end_addr,
	      em_data);

#ifdef POWERPC_LE_PE
      if (eh_handler == 0 && eh_data != 0)
	{
	  /* Special bits here, although the meaning may */
	  /* be a little mysterious. The only one I know */
	  /* for sure is 0x03.                           */
	  /* Code Significance                           */
	  /* 0x00 None                                   */
	  /* 0x01 Register Save Millicode                */
	  /* 0x02 Register Restore Millicode             */
	  /* 0x03 Glue Code Sequence                     */
	  switch (eh_data)
	    {
	    case 0x01:
	      fprintf(file, " Register save millicode");
	      break;
	    case 0x02:
	      fprintf(file, " Register restore millicode");
	      break;
	    case 0x03:
	      fprintf(file, " Glue code sequence");
	      break;
	    default:
	      break;
	    }
	}
#endif	   
      fprintf(file, "\n");
    }

  free (data);

  return true;
}

#define IMAGE_REL_BASED_HIGHADJ 4
static const char *tbl[] =
{
"ABSOLUTE",
"HIGH",
"LOW",
"HIGHLOW",
"HIGHADJ",
"MIPS_JMPADDR",
};

static boolean pe_print_reloc PARAMS ((bfd *, PTR));
static boolean
pe_print_reloc (abfd, vfile)
     bfd *abfd;
     PTR vfile;
{
  FILE *file = (FILE *) vfile;
  bfd_byte *data = 0;
  asection *section = bfd_get_section_by_name (abfd, ".reloc");
  bfd_size_type datasize = 0;
  bfd_size_type i;
  bfd_size_type start, stop;

  if (section == 0)
    return true;

  if (bfd_section_size (abfd, section) == 0)
    return true;

  fprintf(file,
	  "\n\nPE File Base Relocations (interpreted .reloc section contents)\n");

  data = (bfd_byte *) bfd_malloc ((size_t) bfd_section_size (abfd, section));
  datasize = bfd_section_size (abfd, section);
  if (data == NULL && datasize != 0)
    return false;

  bfd_get_section_contents (abfd, 
			    section, 
			    (PTR) data, 0, 
			    bfd_section_size (abfd, section));

  start = 0;

  stop = bfd_section_size (abfd, section);

  for (i = start; i < stop;)
    {
      int j;
      bfd_vma virtual_address;
      long number, size;

      /* The .reloc section is a sequence of blocks, with a header consisting
	 of two 32 bit quantities, followed by a number of 16 bit entries */

      virtual_address = bfd_get_32(abfd, data+i);
      size = bfd_get_32(abfd, data+i+4);
      number = (size - 8) / 2;

      if (size == 0) 
	{
	  break;
	}

      fprintf (file,
	       "\nVirtual Address: %08lx Chunk size %ld (0x%lx) Number of fixups %ld\n",
	       virtual_address, size, size, number);

      for (j = 0; j < number; ++j)
	{
	  unsigned short e = bfd_get_16(abfd, data + i + 8 + j*2);
	  unsigned int t =   (e & 0xF000) >> 12;
	  int off = e & 0x0FFF;

	  if (t >= sizeof(tbl)/sizeof(char *)) 
	    abort();

	  fprintf(file,
		  "\treloc %4d offset %4x [%4lx] %s", 
		  j, off, (long) (off + virtual_address), tbl[t]);

	  /* HIGHADJ takes an argument, but there's not documentation that
	     it does, or what it means.  Inferred from DUMPBIN. */
	  if (t == IMAGE_REL_BASED_HIGHADJ) 
	    {
	       fprintf(file, " (%4x)\n", (unsigned)bfd_get_16(abfd, data + i + 8 + j*2 + 2));
	       j++;
	    }
	    else
	       fprintf(file, "\n");
	  
	}
      i += size;
    }

  free (data);

  return true;
}

#ifdef DYNAMIC_LINKING /* [ */

static boolean pe_print_dynamic PARAMS((bfd *, PTR));
static boolean
pe_print_dynamic (abfd, vfile)
     bfd *abfd;
     PTR vfile;
{
  FILE *f = (FILE *) vfile;
  asection *s;
  bfd_byte *dynbuf = NULL;

  if ((abfd->file_flags & DYNAMIC) == 0)
      return;

  s = coff_dynamic(abfd);
  if (s != NULL)
    {
      unsigned long link;
      coff_external_dyn *extdyn, *extdynend;

      fprintf (f, "\nDynamic (.so) Section:\n");

      dynbuf = (bfd_byte *) bfd_malloc (s->_raw_size);
      if (dynbuf == NULL)
	return false;
      if (! bfd_get_section_contents (abfd, s, (PTR) dynbuf, (file_ptr) 0,
				      s->_raw_size))
	return false;

      link = s->link_index;

      extdyn = (coff_external_dyn *)dynbuf;
      extdynend = extdyn + s->_raw_size / sizeof (coff_external_dyn);

      for (; extdyn < extdynend; extdyn++)
	{
	  coff_internal_dyn dyn;
	  const char *name;
	  char ab[20];
	  boolean stringp;

	  bfd_coff_swap_dyn_in (abfd, (PTR) extdyn, &dyn);

	  if (dyn.d_tag == DT_NULL)
	    break;

	  stringp = false;
	  switch (dyn.d_tag)
	    {
	    default:
	      sprintf (ab, "0x%lx", (unsigned long) dyn.d_tag);
	      name = ab;
	      break;

	    case DT_NEEDED: name = "NEEDED"; stringp = true; break;
	    case DT_PLTRELSZ: name = "PLTRELSZ"; break;
	    case DT_PLTGOT: name = "PLTGOT"; break;
	    case DT_HASH: name = "HASH"; break;
	    case DT_STRTAB: name = "STRTAB"; break;
	    case DT_SYMTAB: name = "SYMTAB"; break;
	    case DT_RELA: name = "RELA"; break;
	    case DT_RELASZ: name = "RELASZ"; break;
	    case DT_RELAENT: name = "RELAENT"; break;
	    case DT_STRSZ: name = "STRSZ"; break;
	    case DT_SYMENT: name = "SYMENT"; break;
	    case DT_INIT: name = "INIT"; break;
	    case DT_FINI: name = "FINI"; break;
	    case DT_SONAME: name = "SONAME"; stringp = true; break;
	    case DT_RPATH: name = "RPATH"; stringp = true; break;
	    case DT_SYMBOLIC: name = "SYMBOLIC"; break;
	    case DT_REL: name = "REL"; break;
	    case DT_RELSZ: name = "RELSZ"; break;
	    case DT_RELENT: name = "RELENT"; break;
	    case DT_PLTREL: name = "PLTREL"; break;
	    case DT_DEBUG: name = "DEBUG"; break;
	    case DT_TEXTREL: name = "TEXTREL"; break;
	    case DT_JMPREL: name = "JMPREL"; break;
	    case DT_AUXILIARY: name = "AUXILIARY"; stringp = true; break;
	    case DT_FILTER: name = "FILTER"; stringp = true; break;
	    case DT_VERSYM: name = "VERSYM"; break;
	    case DT_VERDEF: name = "VERDEF"; break;
	    case DT_VERDEFNUM: name = "VERDEFNUM"; break;
	    case DT_VERNEED: name = "VERNEED"; break;
	    case DT_VERNEEDNUM: name = "VERNEEDNUM"; break;
	    }

	  fprintf (f, "  %-11s ", name);
	  if (! stringp)
	    fprintf (f, "0x%lx", (unsigned long) dyn.d_un.d_val);
	  else
	    {
	      const char *string;

	      string = bfd_coff_string_from_coff_section (abfd, link,
							dyn.d_un.d_val);
	      if (string == NULL)
		 string = "<BAD STRING>";
	      fprintf (f, "%s", string);
	    }
	  fprintf (f, "\n");
	}

      free (dynbuf);
      dynbuf = NULL;
    }

  if ((coff_dynverdef (abfd) != 0 && dyn_data(abfd)->verdef == NULL)
      || (coff_dynverref (abfd) != 0 && dyn_data(abfd)->verref == NULL))
    {
      if (! _bfd_coff_slurp_version_tables (abfd))
	return false;
    }

  if (coff_dynverdef (abfd) != 0)
    {
      coff_internal_verdef *t;

      fprintf (f, "\nVersion definitions:\n");
      for (t = dyn_data(abfd)->verdef; t != NULL; t = t->vd_nextdef)
	{
	  fprintf (f, "%d 0x%2.2x 0x%8.8lx %s\n", t->vd_ndx,
		   t->vd_flags, t->vd_hash, t->vd_nodename);
	  if (t->vd_auxptr->vda_nextptr != NULL)
	    {
	      coff_internal_verdaux *a;

	      fprintf (f, "\t");
	      for (a = t->vd_auxptr->vda_nextptr;
		   a != NULL;
		   a = a->vda_nextptr)
		fprintf (f, "%s ", a->vda_nodename);
	      fprintf (f, "\n");
	    }
	}
    }

  if (coff_dynverref (abfd) != 0)
    {
      coff_internal_verneed *t;

      fprintf (f, "\nVersion References:\n");
      for (t = dyn_data(abfd)->verref; t != NULL; t = t->vn_nextref)
	{
	  coff_internal_vernaux *a;

	  fprintf (f, "  required from %s:\n", t->vn_filename);
	  for (a = t->vn_auxptr; a != NULL; a = a->vna_nextptr)
	    fprintf (f, "    0x%8.8lx 0x%2.2x %2.2d %s\n", a->vna_hash,
		     a->vna_flags, a->vna_other, a->vna_nodename);
	}
    }
    return true;
}

#endif /* ] */

/* Print out the program headers.  */

boolean pe_print_private_bfd_data PARAMS((bfd *, PTR));

boolean
pe_print_private_bfd_data (abfd, vfile)
     bfd *abfd;
     PTR vfile;
{
  FILE *file = (FILE *) vfile;
  int j;
  pe_data_type *pe = pe_data (abfd);
  struct internal_extra_pe_aouthdr *i = &pe->pe_opthdr;

  /* ctime implies \n */
  fprintf (file, "\nTime/Date\t\t%s", ctime(&pe->coff.datestamp));
  fprintf (file,"\nImageBase\t\t");
  fprintf_vma (file, i->ImageBase);
  fprintf (file,"\nSectionAlignment\t");
  fprintf_vma (file, i->SectionAlignment);
  fprintf (file,"\nFileAlignment\t\t");
  fprintf_vma (file, i->FileAlignment);
  fprintf (file,"\nMajorOSystemVersion\t%d\n", i->MajorOperatingSystemVersion);
  fprintf (file,"MinorOSystemVersion\t%d\n", i->MinorOperatingSystemVersion);
  fprintf (file,"MajorImageVersion\t%d\n", i->MajorImageVersion);
  fprintf (file,"MinorImageVersion\t%d\n", i->MinorImageVersion);
  fprintf (file,"MajorSubsystemVersion\t%d\n", i->MajorSubsystemVersion);
  fprintf (file,"MinorSubsystemVersion\t%d\n", i->MinorSubsystemVersion);
  fprintf (file,"Reserved1\t\t%08lx\n", i->Reserved1);
  fprintf (file,"SizeOfImage\t\t%08lx\n", i->SizeOfImage);
  fprintf (file,"SizeOfHeaders\t\t%08lx\n", i->SizeOfHeaders);
  fprintf (file,"CheckSum\t\t%08lx\n", i->CheckSum);
  fprintf (file,"Subsystem\t\t%08x\n", i->Subsystem);
  fprintf (file,"DllCharacteristics\t%08x\n", i->DllCharacteristics);
  fprintf (file,"SizeOfStackReserve\t");
  fprintf_vma (file, i->SizeOfStackReserve);
  fprintf (file,"\nSizeOfStackCommit\t");
  fprintf_vma (file, i->SizeOfStackCommit);
  fprintf (file,"\nSizeOfHeapReserve\t");
  fprintf_vma (file, i->SizeOfHeapReserve);
  fprintf (file,"\nSizeOfHeapCommit\t");
  fprintf_vma (file, i->SizeOfHeapCommit);
  fprintf (file,"\nLoaderFlags\t\t%08lx\n", i->LoaderFlags);
  fprintf (file,"NumberOfRvaAndSizes\t%08lx\n", i->NumberOfRvaAndSizes);

  fprintf (file,"\nThe Data Directory\n");
  for (j = 0; j < IMAGE_NUMBEROF_DIRECTORY_ENTRIES; j++) 
    {
      fprintf (file, "Entry %1x ", j);
      fprintf_vma (file, i->DataDirectory[j].VirtualAddress);
      fprintf (file, " %08lx ", i->DataDirectory[j].Size);
      fprintf (file, "%s\n", dir_names[j]);
    }

  pe_print_idata(abfd, vfile);
  pe_print_edata(abfd, vfile);
#ifdef DYNAMIC_LINKING
  pe_print_dynamic(abfd, vfile);
#endif
  pe_print_pdata(abfd, vfile);
  pe_print_reloc(abfd, vfile);

  return true;
}

/* Copy any private info we understand from the input bfd
   to the output bfd.  */

boolean
pei_bfd_copy_private_bfd_data (ibfd, obfd)
     bfd *ibfd, *obfd;
{
  /* One day we may try to grok other private data.  */
  if (ibfd->xvec->flavour != bfd_target_coff_flavour
      || obfd->xvec->flavour != bfd_target_coff_flavour)
    return true;

  pe_data(obfd)->pe_opthdr = pe_data (ibfd)->pe_opthdr;

  /* for strip(1): if we removed .reloc, we'll make a real mess of things
     if we don't remove this entry as well */
  if (!pe_data (obfd)->has_reloc_section) {
      pe_data(obfd)->pe_opthdr.DataDirectory[5].VirtualAddress = 0;
      pe_data(obfd)->pe_opthdr.DataDirectory[5].Size = 0;
  }

  return true;
}

/* Copy private section data.  Only PEI (not PE) uses this. */
boolean
pei_bfd_copy_private_section_data (ibfd, isec, obfd, osec)
     bfd *ibfd;
     asection *isec;
     bfd *obfd;
     asection *osec;
{
  if (bfd_get_flavour (ibfd) != bfd_target_coff_flavour
      || bfd_get_flavour (obfd) != bfd_target_coff_flavour)
    return true;

  if (coff_section_data (ibfd, isec) != NULL
      && pei_section_data (ibfd, isec) != NULL)
    {
      if (coff_section_data (obfd, osec) == NULL)
	{
	  osec->used_by_bfd =
	    (PTR) bfd_zalloc (obfd, sizeof (struct coff_section_tdata));
	  if (osec->used_by_bfd == NULL)
	    return false;
	}
      if (pei_section_data (obfd, osec) == NULL)
	{
	  coff_section_data (obfd, osec)->tdata =
	    (PTR) bfd_zalloc (obfd, sizeof (struct pei_section_tdata));
	  if (coff_section_data (obfd, osec)->tdata == NULL)
	    return false;
	}
      pei_section_data (obfd, osec)->virt_size =
	pei_section_data (ibfd, isec)->virt_size;
    }
#ifdef DYNAMIC_LINKING
    osec->info_r = isec->info_r;
    osec->info_l = isec->info_l;
#endif

  return true;
}

#ifdef DYNAMIC_LINKING

#define LOADER_SYM "__LOADER_NAME"

boolean
pei_generic_size_dynamic_sections (output_bfd, info)
     bfd *output_bfd;
     struct bfd_link_info *info;
{
  bfd *dynobj;
  asection *s;
  boolean plt;
  boolean relocs;
  boolean reltext;

  dynobj = coff_hash_table (info)->dynobj;

  /* We can get here because we're looking to see if any PIC modules
     got in a static main program.  If none did, dynobj will be null,
     and we're done.  */
  if (dynobj == 0)
      return;

  if (coff_hash_table (info)->dynamic_sections_created)
    {
      /* Set the contents of the .interp section to the interpreter.  */
      if (! info->shared)
	{
          struct coff_link_hash_entry *h = NULL;

	  s = bfd_get_section_by_name (dynobj, ".interp");
	  BFD_ASSERT (s != NULL);
	  s->_raw_size = sizeof LOADER_NAME;
	  s->contents = (unsigned char *) LOADER_NAME;

	  /* Define the symbol __LOADER_NAME_ at the start of the
	     .plt section.  */
	  if (! (bfd_coff_link_add_one_symbol
		 (info, dynobj, 
		  &LOADER_SYM[bfd_get_symbol_leading_char(dynobj)=='_'?0:1],
		  BSF_GLOBAL, s,
		  (bfd_vma) 0, (const char *) NULL, 
		  false, false,
		  (struct bfd_link_hash_entry **) &h)))
	    return false;
	  h->coff_link_hash_flags |= COFF_LINK_HASH_DEF_REGULAR;
	  h->type = 0;
	}
    }
  else
    {
      /* We may have created entries in the .rel.got section.
         However, if we are not creating the dynamic sections, we will
         not actually use these entries.  Reset the size of .rel.got,
         which will cause it to get stripped from the output file
         below.  */
      s = bfd_get_section_by_name (dynobj, ".rel.got");
      if (s != NULL)
	s->_raw_size = 0;
    }

  /* The check_relocs and adjust_dynamic_symbol entry points have
     determined the sizes of the various dynamic sections.  Allocate
     memory for them.  */
  plt = false;
  relocs = false;
  reltext = false;
  for (s = dynobj->sections; s != NULL; s = s->next)
    {
      const char *name;
      boolean strip;

      if ((s->flags & SEC_LINKER_CREATED) == 0)
	continue;

      /* It's OK to base decisions on the section name, because none
	 of the dynobj section names depend upon the input files.  */
      name = bfd_get_section_name (dynobj, s);

      strip = false;

      if (strcmp (name, ".plt") == 0)
	{
	  if (s->_raw_size == 0)
	    {
	      /* Strip this section if we don't need it; see the
                 comment below.  */
	      strip = true;
	    }
	  else
	    {
	      /* Remember whether there is a PLT.  */
	      plt = true;
	    }
	}
      else if (strncmp (name, ".rel", 4) == 0)
	{
	  if (s->_raw_size == 0)
	    {
	      /* If we don't need this section, strip it from the
		 output file.  This is mostly to handle .rel.bss and
		 .rel.plt.  We must create both sections in
		 create_dynamic_sections, because they must be created
		 before the linker maps input sections to output
		 sections.  The linker does that before
		 adjust_dynamic_symbol is called, and it is that
		 function which decides whether anything needs to go
		 into these sections.  */
	      strip = true;
	    }
	  else
	    {
	      asection *target;

	      /* Remember whether there are any reloc sections other
                 than .rel.plt.  */
	      if (strcmp (name, ".rel.plt") != 0)
		{
		  const char *outname;

		  relocs = true;

		  /* We're left with .rel.internal and .rel.got.
		     If there is a GOT (there may not be), it is
		     by definition already read-write.  That leaves
		     rel.internal to reflect relocations in all other
		     sections; if it exists (and is non-zero size) then
		     there are relocations that will (probably) cause 
		     the text section to be written. It would be nice
		     to be more accurate in this estimation.  That would
		     speed up runtime startup. */
		     
	          if (strcmp (name, ".rel.internal") == 0)
		    reltext = true;
		}

	      /* We use the reloc_count field as a counter if we need
		 to copy relocs into the output file.  */
	      s->reloc_count = 0;
	    }
	}
      else if (strncmp (name, ".got", 4) != 0)
	{
	  /* It's not one of our sections, so don't allocate space.  */
	  continue;
	}

      if (strip && strcmp(s->output_section->name,s->name)==0)
	{
	  asection **spp;

	  for (spp = &s->output_section->owner->sections;
	       *spp != s->output_section;
	       spp = &(*spp)->next)
	    ;
	  *spp = s->output_section->next;
	  --s->output_section->owner->section_count;

	  continue;
	}

      /* Allocate memory for the section contents.  */
      s->contents = (bfd_byte *) bfd_alloc (dynobj, s->_raw_size);
      if (s->contents == NULL && s->_raw_size != 0)
	return false;
    }

  if (coff_hash_table (info)->dynamic_sections_created)
    {
      /* Add some entries to the .dynamic section.  We fill in the
	 values later, in coff_i386_finish_dynamic_sections, but we
	 must add the entries now so that we get the correct size for
	 the .dynamic section.  The DT_DEBUG entry is filled in by the
	 dynamic linker and used by the debugger.  */
      if (! info->shared)
	{
	  if (! coff_add_dynamic_entry (info, DT_DEBUG, 0))
	    return false;
	}

      if (plt)
	{
	  if (! coff_add_dynamic_entry (info, DT_PLTGOT, 0)
	      || ! coff_add_dynamic_entry (info, DT_PLTRELSZ, 0)
	      || ! coff_add_dynamic_entry (info, DT_PLTREL, DT_REL)
	      || ! coff_add_dynamic_entry (info, DT_JMPREL, 0))
	    return false;
	}

      if (relocs)
	{
	  if (! coff_add_dynamic_entry (info, DT_REL, 0)
	      || ! coff_add_dynamic_entry (info, DT_RELSZ, 0)
	      || ! coff_add_dynamic_entry (info, DT_RELENT, RELSZ))
	    return false;
	}

      if (reltext)
	{
	  if (! coff_add_dynamic_entry (info, DT_TEXTREL, 0))
	    return false;
	}
    }

  return true;
}

/* Used in the final link pass to sort relocations; since the relocations
   have been swapped out, it's somewhat target dependent. */
#ifdef DYNAMIC_LINKING
/* Used below to sort dynamic relocations into symbol table order */
int
reloc_compar(const void *av, const void *bv)
{
    struct external_reloc *a = (struct external_reloc *)av;
    struct external_reloc *b = (struct external_reloc *)bv;

    if (((*(int *)&a->r_symndx)&0xffffff) < ((*(int *)&b->r_symndx)&0xffffff))
       return -1;
    if (((*(int *)&a->r_symndx)&0xffffff) > ((*(int *)&b->r_symndx)&0xffffff))
       return 1;
    return 0;
}
#endif

#endif

