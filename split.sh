#!/bin/bash

rm [0-9A-F]*.log >/dev/null 2>&1
IFS=$'\n'
sed -e 's/.\(# [^{]*{[0-9A-F]\{8,\}}\)/\n\1/g' >/tmp/$$$$
for pid in `cat /tmp/$$$$ | cut -f 2 -d\{ | cut -f 1 -d\} | grep ^[0-9A-F]*$ | sort | uniq`; do
	grep $pid /tmp/$$$$ >$pid.log
done
rm /tmp/$$$$
