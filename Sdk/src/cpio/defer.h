
/******************
SSIRCSid[]="$Header: /E/interix/src/cpio/defer.h,v 1.2 1997/10/09 15:30:47 SSI_DEV+bruce Exp $";
*/


struct deferment
  {
    struct deferment *next;
    struct new_cpio_header header;
  };

struct deferment *create_deferment P_((struct new_cpio_header *file_hdr));
void free_deferment P_((struct deferment *d));
