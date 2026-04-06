#!/bin/sh

die() {
    echo "error: $1"
    exit 1
}

# validate image path
if [ -z "$1" ]; then
    die "no image provided"
fi
if [ ! -e "$1" ]; then
    die "cant find $1"
fi

WORKDIR="/data"
if [ ! -e "$WORKDIR" ]; then
    # not running under stock firmware?
    # fall back to root
    echo "/data doesnt exist (non-stock firmware?)"
    echo "working out of /tmp instead!"
    WORKDIR="/tmp"
fi
IMG_BACKUP="$WORKDIR/bastic_fact.bak"
IMG_PATCHED="$WORKDIR/bastic_fact.bin"
LEAVE_ME_ALONE="$WORKDIR/bootastic/.restore"

# make sure work directory is writable
touch "$WORKDIR/.write_test" || die "$WORKDIR is not writable"
rm "$WORKDIR/.write_test"

# locate bootastic + factory provisioning partition
if [ ! -e "/proc/mtd" ]; then
    die "no /proc/mtd? (hint: this script is not for your host!)"
fi
BASTIC_FACT="/dev/mtd$(cat /proc/mtd | grep bastic_fact | cut -d: -f1 | cut -c 4-)"
if [ ! -e "$BASTIC_FACT" ]; then
    die "could not locate bastic_fact!"
fi

# dont overwrite an existing backup
if [ ! -e "$IMG_BACKUP" ]; then
    echo "backing up original bastic_fact partition to $IMG_BACKUP..."
    # dump the entire MTD partition
    dd if="$BASTIC_FACT" of="$IMG_BACKUP"
else
    echo "$IMG_BACKUP already exists, skipping backup!"
fi

echo "patching bastic_fact..."

# copy backup image to new working image
cp "$IMG_BACKUP" "$IMG_PATCHED"

# write new preloader to the start of the image
# leaves some leftover garbage after our new preloader, shouldnt matter though
cat < "$1" 1<> "$IMG_PATCHED"

# well, let's go for it!
flash_erase "$BASTIC_FACT" 0 0
nandwrite "$BASTIC_FACT" "$IMG_PATCHED"

# keep stock firmware from trying to update bootastic
mkdir -p "$WORKDIR/bootastic"
touch "$LEAVE_ME_ALONE"
