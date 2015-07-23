#!/bin/sh

which qemu-i386 > /dev/null || {
	echo "You need to install qemu-i386 to run this script. On debian, run:"
	echo "sudo apt-get install qemu-user"
	exit 1
}

which wget > /dev/null || {
	echo "You need to install wget to run this script. On debian, run:"
	echo "sudo apt-get install wget"
	exit 1
}

PATH=`pwd`:$PATH
which proot > /dev/null || {
	echo "Please compile proot first, and copy it to the current directory"
	exit 1
}

PKGS=`cat slackware-devel.txt | tr '\n' ',' | sed 's@,@*,@g' | sed 's/,[*]$//' | sed 's/,$//'`
echo PKGS "$PKGS"

# Get Slackware/ARM packages:
for DIR in a ap d e l n tcl; do
	wget -r -np -N --accept="$PKGS" http://ftp.slackware.com/pub/slackware/slackware-14.1/slackware/$DIR/ || exit 1
done

rm -rf slackwarex86-14.1
mkdir slackwarex86-14.1 || exit 1

# Extract only a minimal subset (ignore errors):
for DIR in a l; do
	ls ftp.slackware.com/pub/slackware/slackware-14.1/slackware/$DIR/*.t?z | xargs -n 1 tar -C slackwarex86-14.1 -x --exclude="dev/*" --exclude="lib/udev/devices/*" -f || exit 1
done

# Do a minimal post-installation setup:
mv slackwarex86-14.1/lib/incoming/* slackwarex86-14.1/lib/ || exit 1
mv slackwarex86-14.1/bin/bash4.new slackwarex86-14.1/bin/bash || exit 1
proot -q qemu-i386 -S slackwarex86-14.1 /sbin/ldconfig || exit 1
proot -q qemu-i386 -r slackwarex86-14.1 ln -s /bin/bash /bin/sh || exit 1

# Install all package correcty (ignore warnings):
ls ftp.slackware.com/pub/slackware/slackware-14.1/slackware/*/*.t?z | xargs -n 1 proot -q qemu-i386 -S slackwarex86-14.1 -b ftp.arm.slackware.com /sbin/installpkg || exit 1
