#include <R_ext/Rdynload.h>
#include "bamfile.h"
#include "io_sam.h"

#ifdef _WIN32
#include "samtools/knetfile.h"
#endif

static const R_CallMethodDef callMethods[] = {
    /* bamfile.c */
    {".bamfile_init", (DL_FUNC) &bamfile_init, 0},
    {".bamfile_open", (DL_FUNC) &bamfile_open, 3},
    {".bamfile_close", (DL_FUNC) &bamfile_close, 1},
    {".bamfile_isopen", (DL_FUNC) &bamfile_isopen, 1},
    {".read_bamfile_header", (DL_FUNC) &read_bamfile_header, 1},
    {".scan_bamfile", (DL_FUNC) &scan_bamfile, 9},
    {".count_bamfile", (DL_FUNC) &count_bamfile, 4},
    {".filter_bamfile", (DL_FUNC) & filter_bamfile, 6},
    /* io_sam.c */
    {".scan_bam_template", (DL_FUNC) &scan_bam_template, 1},
    {".scan_bam_cleanup", (DL_FUNC) &scan_bam_cleanup, 0},
    {".index_bam", (DL_FUNC) &index_bam, 1},
    {".sort_bam", (DL_FUNC) &sort_bam, 4},
    {NULL, NULL, 0}
};

void
R_init_Rsamtools(DllInfo *info)
{
#ifdef _WIN32
    int status = knet_win32_init();
    if (status != 0)
      Rf_error("internal: failed to initialize Winsock; error %d",
               status);
#endif
    R_registerRoutines(info, NULL, callMethods, NULL, NULL);
}

void
R_unload_Rsamtools(DllInfo *info)
{
#ifdef _WIN32
    knet_win32_destroy();
#endif
}
