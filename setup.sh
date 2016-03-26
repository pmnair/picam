#!/bin/sh

if [ "$(id -u)" != "0" ]; then
	echo "Please run the script with sudo."
	exit 1
fi

media_drv=$1

setup_storage_mount() {
	mkdir -p /media/data
	if [ ! -z $media_drv ]
	then
		media_uuid_str=`blkid $media_drv | awk '{print $3}'`
		echo $media_uuid_str

		grep -q "/media/data" /etc/fstab
		if [ $? -eq 1 ]
		then
			echo "$media_uuid_str	/media/data	vfat	default	0	2" >> /etc/fstab
			mount -a
		else
			echo "[!] /etc/fstab contains  an old entry for /media/data; please"
			echo "    review and remove before proceeding"
			exit 1
		fi
	fi
}

setup_autorun() {
	grep -q "pi/picam/picam-run" /etc/rc.local
	if [ $? -eq 1 ]
	then
		sed -i '/exit 0$/i /home/pi/picam/picam-run > /dev/null &' /etc/rc.local
	fi
}

setup_smb_share() {
	grep -q "    path = /media/data" /etc/samba/smb.conf
	if [ $? -eq 1 ]
	then
		systemctl list-unit-files | grep -q smbd.service
		if [ $? -eq 0 ]
		then
			# add configuration to export /media/data
			echo "[media]" >> /etc/samba/smb.conf
			echo "    path = /media/data" >> /etc/samba/smb.conf
			echo "    browseable = yes" >> /etc/samba/smb.conf
			echo "    read only = yes" >> /etc/samba/smb.conf
			echo "    guest ok = no" >> /etc/samba/smb.conf

			# restart the smb daemon
			service smbd restart
		else
			echo "[!] Samba not installed; please intall and re-run setup"
			exit 1
		fi
		echo "[!] Add smb password for desired user using smbpasswd -a <user>"
	fi
}

# setup the storage drive and
# automount for the storage drive
echo "[*] Setting up storage"
setup_storage_mount

# setup autorun for picam
echo "[*] Setting up picam autorun"
setup_autorun

# setup samba share
echo "[*] Setting up media sharing using smb"
setup_smb_share
