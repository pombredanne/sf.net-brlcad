#!/bin/sh
#                          V L S . S H
# BRL-CAD
#
# Copyright (c) 2010-2012 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
###

# Ensure /bin/sh
export PATH || (echo "This isn't sh."; sh $0 $*; kill $$)

# source common library functionality, setting ARGS, NAME_OF_THIS,
# PATH_TO_THIS, and THIS.
. $1/regress/library.sh

TEST=vls
TESTSCRIPT=$TEST.sh
TESTEXE=test_vls
TESTLOG=${TEST}.log
TESTCMD="`ensearch $TESTEXE`"
if test ! -f "$TESTCMD" ; then
    echo "Unable to find $TESTEXE, aborting"
    exit 1
fi

rm -f $TESTLOG

# the test program outputs expected failues to stderr
# run the test (capture number of expected failures in file $ELOG)
ELOG=t
`$TESTCMD 1> $TESTLOG 2>$ELOG`
STATUS=$?
# STATUS contains number of UNEXPECTED failures
EXP=`cat $ELOG`
# the known number of expected failures is:
KNOWNEXP=3

if [ $STATUS -eq 0 ] ; then
    if [ $EXP -eq $KNOWNEXP ] ; then
        echo "-> $TESTSCRIPT succeeded with $EXP expected failed test(s)."
        echo "   See file './regress/$TESTLOG' for results."
        echo "   Do NOT use the failures in production code."
    elif [ $EXP -ne 0 ] ; then
        echo "-> $TESTSCRIPT succeeded with $EXP expected failed test(s)."
        echo "   But SURPRISE!  We expected $KNOWNEXP failed tests so something has changed!"
        echo "   See file './regress/$TESTLOG' for results and compare"
        echo "       with file './src/libbu/test_vls.c'."
        echo "   Do NOT use the failures in production code."
    else
        echo "-> $TESTSCRIPT succeeded"
        # remove test products
        rm -f $TESTLOG $ELOG
    fi
else
    echo "-> $TESTSCRIPT unexpectedly FAILED $STATUS test(s)."
    echo "   See file './regress/$TESTLOG' for results."
fi

exit $STATUS

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
