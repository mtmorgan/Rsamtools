#include "bamfile.h"
#include "io_sam.h"

static SEXP BAMFILE_TAG = NULL;

samfile_t *
_bam_tryopen(const char *filename, const char *filemode, void *aux)
{
    samfile_t *sfile = samopen(filename, filemode, aux);
    if (sfile == 0)
        Rf_error("failed to open SAM/BAM file\n  file: '%s'", 
		 filename);
    if (sfile->header == 0 || sfile->header->n_targets == 0) {
        samclose(sfile);
        Rf_error("SAM/BAM header missing or empty\n  file: '%s'", 
                 filename);
    }
    return sfile;
}

bam_index_t *
_bam_tryindexload(const char *indexname)
{
    bam_index_t *index = bam_index_load(indexname);
    if (index == 0)
        Rf_error("failed to load BAM index\n  file: %s", indexname);
    return index;
}

static void
_bamfile_checkext(SEXP ext, const char *lbl)
{
    if (EXTPTRSXP != TYPEOF(ext) || 
	BAMFILE_TAG != R_ExternalPtrTag(ext))
	Rf_error("incorrect 'BamFile' for '%s'", lbl);
}

static void
_bamfile_checknames(SEXP filename, SEXP indexname, SEXP filemode)
{
    if (!IS_CHARACTER(filename) || LENGTH(filename) > 1)
        Rf_error("'filename' must be character(0) or character(1)");
    if (!IS_CHARACTER(indexname) || LENGTH(indexname) > 1)
        Rf_error("'indexname' must be character(0) or character(1)");
    if (!IS_CHARACTER(filemode) || LENGTH(filemode) != 1)
        Rf_error("'filemode' must be character(1)");
}

static void
_bamfile_close(SEXP ext)
{
    _BAM_FILE *bfile = BFILE(ext);
    if (NULL != bfile->file)
	samclose(bfile->file);
    if (NULL != bfile->index)
	bam_index_destroy(bfile->index);
    bfile->file = NULL;
    bfile->index = NULL;
}

static void
_bamfile_finalizer(SEXP ext)
{
    if (NULL == R_ExternalPtrAddr(ext))
        return;
    _bamfile_close(ext);
    _BAM_FILE *bfile = BFILE(ext);
    Free(bfile);
    R_SetExternalPtrAddr(ext, NULL);
}

SEXP
bamfile_init()
{
    BAMFILE_TAG = install("BamFile");
    return R_NilValue;
}

SEXP
bamfile_open(SEXP filename, SEXP indexname, SEXP filemode)
{
    _bamfile_checknames(filename, indexname, filemode);

    _BAM_FILE *bfile = (_BAM_FILE *) Calloc(1, _BAM_FILE);

    bfile->file = NULL;
    if (0 != Rf_length(filename)) {
	const char *cfile = translateChar(STRING_ELT(filename, 0));
	bfile->file = 
	    _bam_tryopen(cfile, CHAR(STRING_ELT(filemode, 0)), 0);
	if ((bfile->file->type & 1) != 1) {
	    samclose(bfile->file);
	    Free(bfile);
	    Rf_error("'filename' is not a BAM file\n  file: %s", cfile);
	}
    }

    bfile->index = NULL;
    if (0 != Rf_length(indexname)) {
	const char *cindex = translateChar(STRING_ELT(indexname, 0));
	bfile->index = _bam_tryindexload(cindex);
	if (NULL == bfile->index) {
	    samclose(bfile->file);
	    Free(bfile);
	    Rf_error("failed to open BAM index\n  index: %s\n", cindex);
	}
    }

    SEXP ext = PROTECT(R_MakeExternalPtr(bfile, BAMFILE_TAG, filename));
    R_RegisterCFinalizerEx(ext, _bamfile_finalizer, TRUE);
    UNPROTECT(1);

    return ext;
}

SEXP
bamfile_close(SEXP ext)
{
    _bamfile_checkext(ext, "close");
    _bamfile_close(ext);
    return ext;
}

SEXP
bamfile_reopen(SEXP ext, SEXP filename, SEXP indexname, SEXP filemode)
{
    /* open within existing externalptr */
    _bamfile_checkext(ext, "reopen");
    _bamfile_close(ext);

    _BAM_FILE *bfile = BFILE(ext);
    if (0 != Rf_length(filename)) {
	const char *cname = translateChar(STRING_ELT(filename, 0));
	const char *cmode = translateChar(STRING_ELT(filemode, 0));
	bfile->file = _bam_tryopen(cname, cmode, 0);
    }
    if (0 != Rf_length(indexname)) {
	const char *cindex = translateChar(STRING_ELT(indexname, 0));
	bfile->index = _bam_tryindexload(cindex);
    }

    return ext;
}

SEXP
bamfile_isopen(SEXP ext)
{
    _bamfile_checkext(ext, "isOpen");
    return NULL != BFILE(ext)->file ? 
	ScalarLogical(TRUE) : ScalarLogical(FALSE);
}

/* implementation */

SEXP
read_bamfile_header(SEXP ext)
{
    _bamfile_checkext(ext, "scanBam");
    _read_bam_header(ext);
}

SEXP
scan_bamfile(SEXP ext, SEXP space, SEXP keepFlags, SEXP isSimpleCigar,
	     SEXP filename, SEXP indexname, SEXP filemode,
	     SEXP reverseComplement, SEXP template_list)
{
    _bamfile_checknames(filename, indexname, filemode);
    _bamfile_checkext(ext, "scanBam");
    _scan_check_params(space, keepFlags, isSimpleCigar);
    if (!(IS_LOGICAL(reverseComplement) &&
          (1L == LENGTH(reverseComplement))))
        Rf_error("'reverseComplement' must be logical(1)");
    _scan_check_template_list(template_list);
    return _scan_bam(ext, space, keepFlags, isSimpleCigar, 
		     filename, indexname, filemode, 
		     reverseComplement, template_list);
}

SEXP
count_bamfile(SEXP ext, SEXP space, SEXP keepFlags, SEXP isSimpleCigar)
{
    _bamfile_checkext(ext, "countBam");
    _scan_check_params(space, keepFlags, isSimpleCigar);
    SEXP count = _count_bam(ext, space, keepFlags, isSimpleCigar);
    if (R_NilValue == count)
        Rf_error("'countBam' failed");
    return count;
}

SEXP
filter_bamfile(SEXP ext, SEXP space, SEXP keepFlags, SEXP isSimpleCigar,
	       SEXP fout_name, SEXP fout_mode)
{
    _bamfile_checkext(ext, "filterBam");
    _scan_check_params(space, keepFlags, isSimpleCigar);
    if (!IS_CHARACTER(fout_name) || 1 != LENGTH(fout_name))
        Rf_error("'fout_name' must be character(1)");
    if (!IS_CHARACTER(fout_mode) || 1 != LENGTH(fout_mode))
        Rf_error("'fout_mode' must be character(1)");
    SEXP result = _filter_bam(ext, space, keepFlags, isSimpleCigar,
			      fout_name, fout_mode);
    if (R_NilValue == result)
        Rf_error("'filterBam' failed");
    return result;
}
