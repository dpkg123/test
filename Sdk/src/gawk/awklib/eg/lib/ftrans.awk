# ftrans.awk --- handle data file transitions
#
# user supplies beginfile() and endfile() functions
#
# Arnold Robbins, arnold@gnu.org, Public Domain
# November 1992

FNR == 1 {
    if (_filename_ != "")
        endfile(_filename_)
    _filename_ = FILENAME
    beginfile(FILENAME)
}

END  { endfile(_filename_) }
