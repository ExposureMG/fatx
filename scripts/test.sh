#!/bin/bash

IFS=$'\n'

TEST=$1
FATX=./fatx
FBIN=$FATX
FTXT=test.sh

TSUF=${TEST:-main}

SIZE=300
WAIT=3
TIMEOUT=60
TABLE=file
PARTITION=x2
NFILE=5

DSK=disk.${TSUF}.fatx
DIF=disk.${TSUF}.dif
MNT=mnt.fatx.${TSUF}
FUSE=
FUSELOG=fuse.${TSUF}.log

prepare() {
	echo Prepare context
	[ -d $MNT ] && fusermount -u $MNT 2>/dev/null
	rm -f "$DSK" "$DIF"
	rm -f "$FUSELOG"
	[ -d $MNT ] || mkdir $MNT
	chmod 0777 "$MNT" 2>/dev/null || true
#	[ -e $DSK ] || dd if=<(yes $'\xFF' | tr -d "\n") of=$DSK bs=$((1024*1024)) count=$SIZE iflag=fullblock
	[ -e $DSK ] || dd if=/dev/urandom of=$DSK bs=$((1024*1024)) count=$SIZE iflag=fullblock
	REF=$(basename $DSK).ref
	[ -z $DIF ] || [ -e $DIF ] || touch $DIF
	DISK=(--table $TABLE --partition $PARTITION $DSK)
	[ -z $DIF ] || DISK+=(--diff $DIF)
}
prefuse() {
	if ! [ -c /dev/fuse ]; then
		exit 77
	fi
	$FATX --as fuse -f ${DISK[@]} $MNT/ $1 </dev/null >"$FUSELOG" 2>&1 &
	FUSE=$!
	count=0
	while ! `df $MNT | grep -q fatx`; do
		sleep 1
		let count++
		if [ $((count)) == $TIMEOUT ]; then
			echo Failed to mount disk
			kilfuse
			close
			exit 1
		fi
	done
}
remfuse() {
	count=0
	while grep -qs " $MNT " /proc/mounts; do
		fusermount -u "$MNT" 2>/dev/null || true
		sleep 1
		let count++
		if [ $((count)) == $TIMEOUT ]; then
			fusermount -uz "$MNT" 2>/dev/null || true
			sleep 1
			if grep -qs " $MNT " /proc/mounts; then
				echo Failed to unmount disk
				kilfuse
				exit 1
			fi
		fi
	done
	if [ -n "$FUSE" ]; then
		kill "$FUSE" 2>/dev/null || true
		wait "$FUSE" 2>/dev/null || true
	fi
	FUSE=
}
kilfuse() {
	kill -9 $FUSE 2>/dev/null
	FUSE=
	fusermount -u $MNT
	rmdir $MNT
}
close() {
	rmdir $MNT
	[ -e $DSK ] && rm $DSK
	[ -z $DIF ] || ([ -e $DIF ] && rm $DIF)
	[ -z $REF ] || ([ -e $REF ] && rm $REF)
	[ -e $FUSELOG ] && rm $FUSELOG
	echo -n
}

mkfs1() {
	echo Mkfs: make disk:
	$FATX --as mkfs -y ${DISK[@]}
	if [ $? == 0 ]; then
		echo "*** Test OK"
	else
		echo "### Test KO"
		exit 1
	fi
}
fuse1() {
	echo Fuse: simple file creation:
	prefuse
	cp $FBIN $MNT/fatx
	cmp -b $FBIN $MNT/fatx
	if [ $? != 0 ]; then
		echo "### Test KO"
		kilfuse
		exit 1
	fi
	cp $FTXT $MNT/fatx
	cmp -b $FTXT $MNT/fatx
	if [ $? == 0 ]; then
		echo "*** Test OK"
	else
		echo "### Test KO"
		kilfuse
		exit 1
	fi
	remfuse
}
fuse2() {
	echo Fuse: moving file:
	prefuse
	mkdir $MNT/dir1
	mkdir $MNT/dir2
	cp $FBIN $MNT/
	mv -f $MNT/fatx $MNT/fatx.bis
	mv -f $MNT/fatx.bis $MNT/dir1/
	mv -f $MNT/dir1 $MNT/dir3
	mv -f $MNT/dir3 $MNT/dir2/
	ls $MNT/dir2/dir3/fatx.bis >/dev/null 2>&1
	if [ $? == 0 ]; then
		echo "*** Test OK"
	else
		echo "### Test KO"
		kilfuse
		exit 1
	fi
	remfuse
}
fuse3() {
	echo Fuse: removing file:
	prefuse
	mkdir $MNT/test
	cp $FBIN $MNT/test
	rm $MNT/test/fatx
	rmdir $MNT/test
	ls $MNT/test >/dev/null 2>&1
	if [ $? != 0 ]; then
		echo "*** Test OK"
	else
		echo "### Test KO"
		kilfuse
		exit 1
	fi
	remfuse
}
fuse4() {
	echo Fuse: multiple file access:
	prefuse
	nmax=5
	for ((n = 1; n <= $nmax; n++)); do
		cp $FTXT $MNT/m$n
	done
	exec 3<>$MNT/m1
	exec 4<>$MNT/m2
	exec 5<>$MNT/m3
	exec 6<$MNT/m4
	exec 7>$MNT/m5
	exec 7>&-
	exec 6>&-
	exec 5>&-
	exec 4>&-
	exec 3>&-
	echo "*** Test OK"
	remfuse
}
fuse6() {
	echo Fuse: directory copies:
	prefuse
	mkdir $MNT/test
	cp $FBIN $MNT/test/
	mkdir $MNT/test2
	cp -r $MNT/test $MNT/test2
	echo "*** Test OK"
	remfuse
}
fuse7() {
	echo Fuse: simultaneous copies:
	if [ -z $1 ]; then
		prefuse
	fi
	tasks=
	if [ -z $1 ]; then
		nmax=$NFILE
	else
		nmax=$1
	fi
	for ((n = 1; n <= $nmax; n++)); do
		dd if=/dev/urandom of=$MNT/r$n bs=$((2 * 1024 * 1024 + 256)) count=1 >/dev/null 2>&1
		if [ `du -b $MNT/r$n | cut -f 1` != $((2 * 1024 * 1024 + 256)) ]; then
			echo "### Test KO", invalid file size
			if [ -z $1 ]; then
				kilfuse
				exit 1
			fi
		fi
	done
	sleep $WAIT
	for ((n = 1; n <= $nmax; n++)); do
		cp $MNT/r$n $MNT/w$n &
		tasks+=$!" "
		sleep 0.1
	done
	tasks+=$!
	nbtasks=${#tasks[*]}
	error=1
	ended=
	for ((n = 1; $TIMEOUT == 0 || n <= $TIMEOUT; n++)); do
		sleep 1
		for pid in ${tasks}; do
			if (! kill -0 $pid 2>/dev/null) || !(echo $ended | grep -q $pid); then
				ended+=$pid" "
			fi
		done
		if [ ${#ended[*]} == $nbtasks ]; then
			error=0
			break;
		fi
	done
	for ((n = 1; n <= $nmax; n++)); do
		sleep 1
		cmp -b $MNT/r$n $MNT/w$n || {
			echo "### Test KO", files are different
			if [ -z $1 ]; then
				kilfuse
				exit 1
			fi
		}
	done
	if [ $error != 0 ]; then
		echo "### Test KO", timeout reached
		if [ -z $1 ]; then
			kilfuse
			exit 1
		fi
	fi
	echo "*** Test OK"
	if [ -z $1 ]; then
		remfuse
	fi
}
fuse8() {
	echo Fuse: directory max entries:
	prefuse
	maxent=512
	for ((i = 0; i < $maxent; i++)); do
		touch $MNT/ent$i
	done
	remfuse
	prefuse
	if [ `ls $MNT/ent* | wc -w` != $maxent ]; then
		echo "### Test KO"
		kilfuse
		exit 1
	else
		echo "*** Test OK"
	fi
	remfuse
}
wait_recovered_file() {
	local src=$1 dst=$2
	local count=0
	while ! (test -e "$dst" && cmp -b "$src" "$dst" >/dev/null 2>&1); do
		sleep 1
		let count++
		if [ $count -eq $TIMEOUT ]; then
			return 1
		fi
	done
	return 0
}
fuse9() {
	echo Fuse: recover mode:
	prefuse
	cp $FBIN $MNT/tbff
	rm $MNT/tbff
	remfuse
	prefuse -R
	wait_recovered_file "$FBIN" "$MNT/tbff"
	if [ $? == 0 ]; then
		echo "*** Test OK"
	else
		echo "### Test KO", file not found or files are different
		kilfuse
		exit 1
	fi
	remfuse
}
fuse10() {
	echo Fuse: FAT stress:
	prefuse
	mkdir $MNT/test
	avail1=$(df -k $MNT | tail -1 | tr -s ' ' | cut -f3 -d' ')
	nmax=100
	for ((n = 1; n <= $nmax; n++)); do
		size=$[($RANDOM % 1000) + 1]
		dd if=/dev/urandom of=$MNT/test/ent$n bs=$((size * 1024 + 256)) count=1 >/dev/null 2>&1
		if [ `du -b $MNT/test/ent$n | cut -f 1` != $((size * 1024 + 256)) ]; then
			echo "### Test KO", invalid file size
			kilfuse
			exit 1
		fi
	done
	avail2=$(df -k $MNT | tail -1 | tr -s ' ' | cut -f3 -d' ')
	avail3=$(du -k $MNT/test | tail -1 | cut -f1)
	if [ $avail3 == $[$avail2 - $avail1] ]; then
		echo "*** Test OK", Used = $avail3, Occupped = $[$avail2 - $avail1]
	else
		echo "### Test KO", Used = $avail3, Occupped = $[$avail2 - $avail1]
		kilfuse
		exit 1
	fi
	remfuse
}
fuse11() {
	echo Fuse: directory entries stress:
	prefuse
	maxent=1024
	tasks=
	for ((i = 0; i < $maxent; i++)); do
		echo "TEST" >$MNT/ent$i &
		tasks+=($!)
	done
	for job in ${tasks[@]}; do
		wait $job
	done
	remfuse
	prefuse
	if [ `ls $MNT/ent* | wc -w` != $maxent ]; then
		echo "### Test KO. Found $(ls $MNT/ent* | wc -w) files instead of $maxent"
		kilfuse
		exit 1
	else
		echo "*** Test OK"
	fi
	remfuse
}
fuse12() {
	echo Fuse: simultaneous creations:
	prefuse
	maxent=100
	tasks=
	for ((i = 0; i < $maxent; i++)); do
		echo "TEST" >$MNT/ent$i &
		tasks+=($!)
	done
	for job in ${tasks[@]}; do
		wait $job
	done
	remfuse
	prefuse
	if [ `ls $MNT/ent* | wc -w` != $maxent ]; then
		echo "### Test KO"
		kilfuse
		exit 1
	else
		echo "*** Test OK"
	fi
	remfuse
}
fuse13() {
	echo Fuse: check statfs:
	prefuse
	rm -rf $MNT/*
	MIN=$(($(df -k $MNT | tail -1 | tr -s ' ' | cut -f3 -d' ') / 2))
	echo TEST >$MNT/tbff
	[ $(($MIN * 4)) -ge $(df -k $MNT | tail -1 | tr -s ' ' | cut -f3 -d' ') ]
	if [ $? == 0 ]; then
		echo "*** Test OK"
	else
		echo "### Test KO, found "$(df -k $MNT | tail -1 | tr -s ' ' | cut -f3 -d' ')" expected < "$(($MIN * 4))
		kilfuse
		exit 1
	fi
	remfuse
}
fsck1() {
	echo Fsck: sanity check:
	$FATX --as fsck -nv ${DISK[@]}
	if [ $? == 0 ]; then
		echo "*** Test OK"
	else
		echo "### Test KO"
		exit 1
	fi
}
fsck2() {
	echo Fsck: circular reference:
	$FATX --as label ${DISK[@]} -l XBOX -v --do "\
		mkdir,	/test1; \
		mkdir,	/test1/test2; \
		lsfat,	/test1; \
		chcls,	/test1/test2,	3; \
	"
	$FATX --as fsck -av ${DISK[@]}
}
fsck3() {
	echo Fsck: Conflicting entries:
	$FATX --as label ${DISK[@]} -l XBOX -v --do "\
		mkdir,	/test1; \
		mkdir,	/test1/test2; \
		rcp,	$FBIN,	/test1/fatx; \
		lsfat,	/test1/fatx; \
		chcls,	/test1/test2,	9; \
	"
	$FATX --as fsck -av ${DISK[@]}
}
labl1() {
	echo Label: check default name:
	$FATX --as label ${DISK[@]}
	if [ $? == 0 ]; then
		echo "*** Test OK"
	else
		echo "### Test KO"
		exit 1
	fi
}
labl2() {
	echo Label: check noname:
	prefuse
	rm $MNT/name.txt
	remfuse
	$FATX --as label ${DISK[@]}
	if [ $? == 0 ]; then
		echo "*** Test OK"
	else
		echo "### Test KO"
		exit 1
	fi
}
labl3() {
	echo Label: set label
	$FATX --as label ${DISK[@]} disk
	if [ $? == 0 ]; then
		echo "*** Test OK"
	else
		echo "### Test KO"
		exit 1
	fi
}
unrm1() {
	echo Unrm: remote recovery:
	prefuse
	cp $FBIN $MNT/tbff
	rm $MNT/tbff
	remfuse
	$FATX --as unrm -y ${DISK[@]}
	if [ $? != 0 ]; then
		echo "### Test KO", unrm failed
		exit 1
	fi
	prefuse
	wait_recovered_file "$FBIN" "$MNT/tbff"
	if [ $? == 0 ]; then
		echo "*** Test OK"
	else
		echo "### Test KO", file not recovered or files are different
		kilfuse
		exit 1
	fi
	remfuse
}
unrm2() {
	echo Unrm: remote dir. recovery:
	prefuse
	mkdir $MNT/test
	cp $FBIN $MNT/test/tbff
	rm $MNT/test/tbff
	rmdir $MNT/test
	remfuse
	$FATX --as unrm -y ${DISK[@]}
	if [ $? != 0 ]; then
		echo "### Test KO", unrm failed
		exit 1
	fi
	prefuse
	test -d $MNT/test && test -e $MNT/test/tbff && cmp -b $FBIN $MNT/test/tbff
	if [ $? == 0 ]; then
		echo "*** Test OK"
	else
		echo "### Test KO", file not recovered or files are different
		kilfuse
		exit 1
	fi
	remfuse
}
unrm3() {
	echo Unrm: local recovery:
	[ -f ./test/tbff ] && rm -f ./test/tbff && rmdir ./test
	prefuse
	mkdir $MNT/test
	cp $FBIN $MNT/test/tbff
	rm $MNT/test/tbff
	remfuse
	$FATX --as unrm -ly ${DISK[@]}
	if [ $? != 0 ]; then
		echo "### Test KO", unrm failed
		exit 1
	fi
	test -e test/tbff && cmp -b $FBIN test/tbff
	if [ $? == 0 ]; then
		echo "*** Test OK"
		rm test/* && rmdir test
	else
		echo "### Test KO", file not recovered or files are different
		exit 1
	fi
}
unrm4() {
	echo Unrm: lost chain recovery:
	$FATX --as mkfs ${DISK[@]} -vy
	dd if=/dev/urandom of=tbff bs=$((1024 * 1024 + 256)) count=1 >/dev/null 2>&1
	$FATX --as label ${DISK[@]} -l XBOX -v --do "\
		mkdir,	/test; \
		lsfat,	/; \
		lsfat,	/name.txt; \
		lsfat,	/test; \
		rcp,	tbff, /test/tbff.bak; \
		lsfat,	/test/tbff.bak; \
		rm,		/test/tbff.bak; \
		rmdir,	/test; \
		mklost,	30:32; \
		mklost,	40:42; \
		mklost,	67:68; \
		mklost,	100:110; \
		rmfat,	31; \
	"
	$FATX --as fsck ${DISK[@]} -vn
	$FATX --as unrm ${DISK[@]} -vy
	$FATX --as fsck ${DISK[@]} -vn
	$FATX --as label ${DISK[@]} -v --do "\
		lsfat,	/test/tbff.bak; \
		lcp,	/test/tbff.bak, tbff.bak; \
	"
	cmp -b tbff tbff.bak
	if [ $? == 0 ]; then
		echo "*** Test OK"
		rm tbff tbff.bak
	else
		echo "### Test KO", file not recovered or files are different
		exit 1
	fi
}

tests=(
	fuse1
	fuse2
	fuse3
	fuse4
	fuse6
	fuse7
	fuse8
	fuse9
	fuse10
	fuse11
	fuse12
	fuse13
	labl1
	labl2
	labl3
	fsck2
	fsck3
	unrm1
	unrm2
	unrm3
	unrm4
)
purpose=(
	"Fuse simple file creation"
	"Fuse moving file"
	"Fuse removing file"
	"Fuse multiple file access"
	"Fuse directory copies"
	"Fuse simultaneous copies"
	"Fuse directory max entries"
	"Fuse recover mode"
	"Fuse FAT stress"
	"Fuse directory entries stress"
	"Fuse simultaneous creations"
	"Fuse check statfs"
	"Label check default name"
	"Label check noname"
	"Label set label"
	"Fsck circular reference"
	"Fsck conflicting entries"
	"Unrm remote recovery"
	"Unrm remote dir recovery"
	"Unrm local recovery"
	"Unrm lost chain recovery"
)

if [[ $# -eq 0 ]]; then
	for((i = 0; i <  ${#tests[*]}; i++)); do
		[ -n "$res" ] && res=$res";"
		res=$res${purpose[$i]// /_}
	done
	echo -n $res
	exit 0
fi
testr=${tests[$TEST]}
if [ "$testr" != "unrm4" ]; then
	trap kilfuse SIGINT
fi
prepare

tmpout=$(mktemp)
set +e
(
	mkfs1 && $testr && fsck1 && close
) >"$tmpout" 2>&1
rc=$?
set -e

cat "$tmpout"
grep -q "### Test KO" "$tmpout" && rc=1
rm -f "$tmpout"

exit $rc
