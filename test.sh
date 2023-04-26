#!/bin/bash

SIZE=300
WAIT=3
TIMEOUT=60

DSK=disk.$$.fat
#DIF=disk.$$.dif
MNT=mnt.$$

REF=
DISK=
CMDFUSE=

prepare() {
	echo Prepare context
	[ -d $MNT ] || mkdir $MNT
	[ -e $DSK ] || dd if=<(yes $'\xFF' | tr -d "\n") of=$DSK bs=$((1024*1024)) count=$SIZE iflag=fullblock
#	[ -e $DSK ] || dd if=/dev/urandom of=$DSK bs=$((1024*1024)) count=600 iflag=fullblock
	REF=$(basename $DSK .fat).ref
	[ -z $DIF ] || [ -e $DIF ] || (touch $DIF; cp $DSK $REF)
	rm -rf $MNT/* || fusermount -u $MNT && rm -rf $MNT/*
	DISK="$DSK"
	[ -z $DIF ] || DISK="$DISK --diff $DIF"
	CMDFUSE="./fatx --as fuse -d $DISK $MNT/"
}
remove() {
	rm -rf $MNT
}

prefuse() {
	if ! [ -c /dev/fuse ]; then
		exit 77
	fi
	$CMDFUSE $1 2>&1 &
	FUSE=$!
	count=0
	while ! `df $MNT | tail -n1 | cut -f 1 -d\  | grep -q fatx`; do
		sleep 1
		let count++
		if [ "$1" == "" -a $((count)) == $TIMEOUT ]; then
			echo Failed to mount disk
			kilfuse
			exit 1
		fi
	done
}
remfuse() {
	fusermount -u $MNT
	count=0
	while `df $MNT | tail -n1 | cut -f 1 -d\  | grep -q fatx`; do
	        sleep 1
		let count++
		if [ $((count)) == $TIMEOUT ]; then
			echo Failed to unmount disk
			kilfuse
			exit 1
		fi
	done
	FUSE=
}
kilfuse() {
	kill -9 $FUSE 2>/dev/null
	FUSE=
	fusermount -u $MNT
}

mkfs1() {
	echo Mkfs: make disk: 
	./fatx --as mkfs -y $DISK 2>&1
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
	cp fatx $MNT
	cmp -b fatx $MNT/fatx
	if [ $? != 0 ]; then
		echo "### Test KO"
		kilfuse
		exit 1
	fi
	cp Makefile $MNT/fatx
	cmp -b Makefile $MNT/fatx
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
	cp fatx $MNT/
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
	cp fatx $MNT/test
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
		cp Makefile $MNT/m$n
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
fuse5() {
	echo Fuse: concurrent access: 
	prefuse
	./fatx --as fsck $DISK 2>&1
	if [ $? != 0 ]; then
		echo "*** Test OK"
	else
		echo "### Test KO"
		kilfuse
		exit 1
	fi
	remfuse
}
fuse6() {
	echo Fuse: directory copies:
	prefuse
	mkdir $MNT/test
	cp fatx $MNT/test/
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
		nmax=50
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
fuse9() {
	echo Fuse: recover mode:
	prefuse
	cp fatx $MNT/tbff
	rm $MNT/tbff
	remfuse
	prefuse -r
	test -e $MNT/tbff && cmp -b fatx $MNT/tbff
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
		tasks+=$!" "
	done
	for job in $tasks; do
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
		tasks+=$!" "
	done
	for job in $tasks; do
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
	./fatx --as fsck -nv $DISK 2>&1
	if [ $? == 0 ]; then
		echo "*** Test OK"
	else
		echo "### Test KO"
		exit 1
	fi
}
fsck2() {
	echo Fsck: circular reference:
	./fatx --as label $DISK -l XBOX -v --do "\
		mkdir,	/test1; \
		mkdir,	/test1/test2; \
		lsfat,	/test1; \
		chcls,	/test1/test2,	3; \
	"
	./fatx --as fsck -av $DISK 2>&1
}
fsck3() {
	echo Fsck: Conflicting entries:
	./fatx --as label $DISK -l XBOX -v --do "\
		mkdir,	/test1; \
		mkdir,	/test1/test2; \
		rcp,	fatx,	/test1/fatx; \
		lsfat,	/test1/fatx; \
		chcls,	/test1/test2,	9; \
	"
	./fatx --as fsck -av $DISK 2>&1
}

labl1() {
	echo Label: check default name: 
	./fatx --as label $DISK 2>&1
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
	./fatx --as label $DISK 2>&1
	if [ $? == 0 ]; then
		echo "*** Test OK"
	else
		echo "### Test KO"
		exit 1
	fi
}
labl3() {
	echo Label: set label 
	./fatx --as label $DISK disk 2>&1
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
	cp fatx $MNT/tbff
	rm $MNT/tbff
	remfuse
	./fatx --as unrm -y $DISK 2>&1
	if [ $? != 0 ]; then
		echo "### Test KO", unrm failed
		exit 1
	fi
	prefuse
	test -e $MNT/tbff && cmp -b fatx $MNT/tbff
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
	cp fatx $MNT/test/tbff
	rm $MNT/test/tbff
	rmdir $MNT/test
	remfuse
	./fatx --as unrm -y $DISK 2>&1
	if [ $? != 0 ]; then
		echo "### Test KO", unrm failed
		exit 1
	fi
	prefuse
	test -d $MNT/test && test -e $MNT/test/tbff && cmp -b fatx $MNT/test/tbff
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
	[ -e ./tbff ] && rm -f ./tbff
	prefuse
	mkdir $MNT/test
	cp fatx $MNT/test/tbff
	rm $MNT/test/tbff
	remfuse
	./fatx --as unrm -ly $DISK 2>&1
	if [ $? != 0 ]; then
		echo "### Test KO", unrm failed
		exit 1
	fi
	test -e tbff && cmp -b fatx tbff
	if [ $? == 0 ]; then
		echo "*** Test OK"
		rm tbff
	else
		echo "### Test KO", file not recovered or files are different
		exit 1
	fi
}
unrm4() {
	echo Unrm: lost chain recovery: 
	./fatx --as mkfs $DISK -vy
	dd if=/dev/urandom of=tbff bs=$((1024 * 1024 + 256)) count=1 >/dev/null 2>&1
	./fatx --as label $DISK -l XBOX -v --do "\
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
	./fatx --as fsck $DISK -vn
	./fatx --as unrm $DISK -vy
	./fatx --as fsck $DISK -vn
	./fatx --as label $DISK -v --do "\
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

check() {
	cmp -b $DSK $REF
	if [ $? == 0 ]; then
		echo "*** Test OK"
	else
		echo "### Test KO", $DSK has been changed.
		exit 1
	fi
}

close() {
	remove
	if ! [ -z $DIF ]; then
		check
	fi
	[ -e $DSK ] && rm $DSK
	[ -z $DIF ] || ([ -e $DIF ] && rm $DIF)
	[ -z $REF ] || ([ -e $REF ] && rm $REF)
	echo -n
}

tests=(
	fuse1
	fuse2
	fuse3
	fuse4
	fuse5
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
testn=`basename $0`

case $testn in
test.sh)
	for ((i = 0; i < ${#tests[*]}; i++)); do
		[ -e test$i ] || ln -s test.sh test$i
	done
	[ -e analyse.sh ] || ln -s test.sh analyse.sh
	[ -e profile.sh ] || ln -s test.sh profile.sh
	;;
analyse.sh)
	rm [0-9A-F]*.log >/dev/null 2>&1
	IFS=$'\n'
	sed -e 's/.\(# [^{]*{[0-9A-F]\{8,\}}\)/\n\1/g' >/tmp/$$$$
	for pid in `cat /tmp/$$$$ | cut -f 2 -d\{ | cut -f 1 -d\} | grep ^[0-9A-F]*$ | sort | uniq`; do
		grep $pid /tmp/$$$$ >$pid.log
	done
	rm /tmp/$$$$
	;;
profile.sh)
	#PRE=--gen-suppressions=all
	[ -d ./logs ] || mkdir logs
	prepare
	CMDFUSE="./fatx --as fuse $DSK $MNT/"
	./fatx --as mkfs -y $DISK
	valgrind $PRE --tool=memcheck --log-file=./logs/leaks.log $CMDFUSE
	fuse7 70
	fusermount -u $MNT
	./fatx --as mkfs -y $DISK
	valgrind $PRE --tool=helgrind --log-file=./logs/locks1.log $CMDFUSE
	fuse7 70
	fusermount -u $MNT
	./fatx --as mkfs -y $DISK
	valgrind $PRE --tool=exp-dhat --log-file=./logs/heap.log $CMDFUSE
	fuse7 70
	fusermount -u $MNT
	./fatx --as mkfs -y $DISK
	valgrind $PRE --tool=drd --log-file=./logs/locks2.log $CMDFUSE
	fuse7 70
	fusermount -u $MNT
	;;
*)
	testn=${testn/test/}
	testr=${tests[$testn]}
	if [ "$testr" != "unrm4" ]; then
		trap kilfuse SIGINT
	fi
	prepare
	mkfs1 && $testr && fsck1 && close
	;;
esac

