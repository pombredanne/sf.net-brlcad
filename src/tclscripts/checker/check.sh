#!/bin/sh

export LC_ALL=C

dry_run=false
if [ $# -gt 0 ] && [ "$1" = "-d" ]; then
    dry_run=true
    shift
fi

if [ $# -lt 1 ]; then
    echo "Usage: $0 filename.g [objects...]"
    exit 1
fi

db="$1"
sz="512"
pwd=`pwd`
loop="loop"
mged="mged"
rtcheck="rtcheck"


echo ""
echo "CHECKING FOR OVERLAPS"
if $dry_run; then
    echo " (skipping rtcheck)"
fi
echo "====================="
echo ""

DIR=`basename $db 2>&1`.ck
JOB=ck.`basename $db 2>&1`

# stash a working copy
echo "Working on a copy in $DIR"
mkdir -p "$DIR"
cp $db $DIR/$JOB.g
cd $DIR
DB=$JOB.g

if [ $# -lt 2 ]; then
    tops=`$mged -c $DB tops -n 2>&1`
    echo "Processing top-level objects @ $sz:" $tops
else
    shift
    tops="$*"
    echo "Processing objects @ $sz:" $tops
fi

# count how many views (should be 16 views)
total=0
for obj in $tops ; do
    for az in `$loop 0 179 45` ; do
	for el in `$loop 0 179 45` ; do
	    total="`expr $total + 1`"
	done
    done
done


# check the views
if ! $dry_run; then
    rm -f $OBJ.plot3
fi
count=1
for obj in $tops ; do
    OBJ=$JOB.$obj
    for az in `$loop 0 179 45` ; do
	for el in `$loop 0 179 45` ; do
	    echo "[$count/$total]"
	    chmod -f 755 $OBJ.$az.$el.plot3
	    if ! $dry_run; then
		$rtcheck -o $OBJ.$az.$el.plot3 -s $sz -a $az -e $el $DB $obj 2> $OBJ.$az.$el
		cat $OBJ.$az.$el.plot3 >> $OBJ.plot3
	    fi
	    chmod -f 755 $OBJ.$az.$el.plot3
	    count="`expr $count + 1`"
	done
    done
done


# parse out overlap line pairings
rm -f $JOB.pairings
for obj in $tops ; do
    OBJ=$JOB.$obj
    rm -f $OBJ.pairings
    for az in `$loop 0 179 45` ; do
	for el in `$loop 0 179 45` ; do
	    if ! [ -f $OBJ.$az.$el ]; then
		echo "WARNING: $OBJ.$az.$el is MISSING"
	    else
		sed -n '/maximum depth/{
		    s/[<>]//g
		    s/[,:] / /g
		    s/^[[:space:]]*//g
		    s/mm[[:space:]]*$//g
		    p
		    }' $OBJ.$az.$el |
		    cut -f 1,2,3,9 -d ' ' |
		    awk '{if ($2 < $1) { tmp = $1; $1 = $2; $2 = tmp}; print $1, $2, $3 * $4}' >> $OBJ.pairings

		cat $OBJ.pairings >> $JOB.pairings
	    fi
	done
    done
done


# tabulate and sort into unique pairings
lines="`sort $JOB.pairings`"
if test "x$lines" = "x" ; then
    linecount=0
    echo ""
    echo "NO OVERLAPS DETECTED"
    cd $pwd
    exit
else
    linecount=`echo "$lines" | wc -l`
fi

echo "Processing $linecount pairings"

leftprev=""
rightprev=""
totalprev=0
printf "[."
while read line ; do
    left=`echo $line | awk '{print $1}'`
    right=`echo $line | awk '{print $2}'`
    len=`echo $line | awk '{print $3}'`

    printf "."

    # init on first pass
    if test "x$leftprev" = "x" && test "x$rightprev" = "x" ; then
	leftprev=$left
	rightprev=$right
    fi

    # only record when current line is different from prevoius
    if test "x$leftprev" != "x$left" || test "x$rightprev" != "x$right" ; then
	LINES="$LINES
$leftprev $rightprev $totalprev"
	leftprev="$left"
	rightprev="$right"
	totalprev=0
    fi

    totalprev="`echo $totalprev $len | awk '{printf "%.4f", $1 + ($2 / 16)}'`"

done <<EOF
$lines
EOF

# record the last line
if test "x$LINES" = "x" && test "x$leftprev" != "x" ; then
    LINES="$leftprev $rightprev $totalprev"
elif test "x$leftprev" != "x" ; then
    LINES="$LINES
$leftprev $rightprev $totalprev"
fi

printf ".]\n"


# report how many unique pairings
if test "x$LINES" = "x" && test "x$leftprev" = "x" ; then
    overlapcnt=0
    echo ""
    echo "NO UNIQUE PAIRINGS FOUND?"
    cd $pwd
    exit
else
    echo "LINES=[$LINES]"
    overlapcnt=`echo "$LINES" | sort -k3 -n -r | wc -l`
fi
echo "Found $overlapcnt unique pairings"


# check each pairing
sz="`echo $sz | awk '{print $1 / 4}'`"
count=1
if ! $dry_run; then
    rm -f $JOB.more.plot3
fi
while read overlap ; do
    echo "Inspecting overlap [$count/$overlapcnt] @ $sz"

    objs="`echo $overlap | awk '{print $1, $2}'`"

    for az in `$loop 0 179 45` ; do
	for el in `$loop 0 179 45` ; do
	    chmod -f 755 $JOB.$count.$az.$el.plot3
	    if ! $dry_run; then
		$rtcheck -o $JOB.$count.$az.$el.plot3 -s $sz -a $az -e $el $DB $objs 2> $JOB.$count.$az.$el
		cat $JOB.$count.$az.$el.plot3 >> $JOB.more.plot3
	    fi
	    chmod -f 755 $JOB.$count.$az.$el.plot3
	done
    done

    count="`expr $count + 1`"
done <<EOF
`echo "$LINES" | sort -k3 -n -r`
EOF


# summarize the overlaps
echo "Overlap Summary:"
echo "$LINES" | sort -k3 -n -r > $JOB.overlaps
cat $JOB.overlaps


cd $pwd
echo ""
echo "Done."

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
