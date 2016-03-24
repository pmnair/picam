#!/bin/sh

if [ "$(id -u)" != "0" ]; then
	echo "Please run the script with sudo."
	exit 1
fi

media_drv=$1
media_uuid_str=`blkid $media_drv | awk '{print $3}'`
echo $media_uuid_str

mkdir -p /media/data
echo "$media_uuid_str	/media/data	vfat	default	0	2" >> /etc/fstab
mount -a

echo "/home/pi/picam/picam-run > /dev/null &" >> /etc/rc.local


