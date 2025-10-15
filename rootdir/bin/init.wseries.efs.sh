#!/vendor/bin/sh

PATH=/sbin:/system/sbin:/system/bin:/system/xbin:/system/vendor/bin:/system/vendor/xbin:$PATH
export PATH

function wait_for_mount() {
    while ! mountpoint -q $1
    do
    done
}

if [ -f /persist/mfg/inproduction ]; then
    start vtnvfsd
    wait_for_mount /nvram/boardid
    start vendor_prop_loader
    wait_for_mount /nvram/perm
    if [ "$(stat -c%s /nvram/perm/bootcontrolblock)" -lt 4096 ]; then
        dd if=/dev/zero bs=4096 count=1 of=/nvram/perm/bootcontrolblock conv=notrunc
    fi
    LOOP_DEV=$(losetup -f --show --sizelimit 4096 /nvram/perm/bootcontrolblock)
    ln -sf $LOOP_DEV /dev/block/by-name/misc
    ln -sf /dev/block/by-name/misc /misc
    return
fi

cd /data/local/tmp

mkdir efs
mount -t qnx6 -o ro /dev/block/bootdevice/by-name/calwork_b efs
dd if=efs/modem_fs1 of=/dev/block/bootdevice/by-name/modemst1
dd if=efs/modem_fs2 of=/dev/block/bootdevice/by-name/modemst2
dd if=efs/modem_fsg of=/dev/block/bootdevice/by-name/fsg
dd if=efs/modem_fsc of=/dev/block/bootdevice/by-name/fsc
umount efs
rm -rf efs

umount /persist
mkfs.ext4 -F /dev/block/bootdevice/by-name/persist
mount /dev/block/bootdevice/by-name/persist /persist

start nvramd
wait_for_mount /nvram/legacy

start vtnvfsd
wait_for_mount /nvram/boardid
wait_for_mount /nvram/nvuser
wait_for_mount /nvram/perm
wait_for_mount /nvram/prdid

if [ "$(stat -c%s /nvram/perm/bootcontrolblock)" -lt 4096 ]; then
    dd if=/dev/zero bs=4096 count=1 of=/nvram/perm/bootcontrolblock conv=notrunc
fi
LOOP_DEV=$(losetup -f --show --sizelimit 4096 /nvram/perm/bootcontrolblock)
ln -sf $LOOP_DEV /dev/block/by-name/misc
ln -sf /dev/block/by-name/misc /misc

cp /nvram/legacy/by-name/NV_OSSTORE_PIN_NUM /nvram/prdid/pin
cp /nvram/legacy/by-name/NV_OSSTORE_bsn_NUM /nvram/boardid/bsn
cp /nvram/legacy/by-name/NV_CAL_IMEI /nvram/prdid/imei
cp /nvram/legacy/by-name/NV_QNX_MAC_BT /nvram/boardid/macaddressbt
cp /nvram/legacy/by-name/NV_QNX_MAC_Wifi /nvram/boardid/macaddresswlan

mkdir -p /persist/cellular/swoop
chown radio:radio /persist/cellular/
chown radio:2902 /persist/cellular/swoop

mkdir -p /persist/mfg
cp /nvram/legacy/by-name/NV_QNX_ECID /persist/mfg/ecid
cp /nvram/legacy/by-name/NV_OSSTORE_AccelTransformInfo_NUM /persist/mfg/factorycal_accel
cp /nvram/legacy/by-name/NV_OSSTORE_Gyroscope_Factory_Cal_NUM /persist/mfg/factorycal_gyroscope
cp /nvram/legacy/by-name/NV_OSSTORE_Light_Sensor_Factory_Cal_NUM /persist/mfg/factorycal_lightsensor
cp /nvram/legacy/by-name/NV_OSSTORE_ToF_Sensor_Factory_Cal_NUM /persist/mfg/factorycal_tofsensor
echo -en "1 qwerty" > /persist/mfg/keypadlanguage
echo -en "0 black" > /persist/mfg/plastichousing
echo -en "0 black" > /persist/mfg/plastickeypad
cp /nvram/legacy/by-name/NV_OSSTORE_SWPartsList_NUM /persist/mfg/swpartlist
cp /nvram/legacy/by-name/NV_OSSTORE_UNLOCK_ACTIVATION_DATA /persist/mfg/unlockcode
echo -en "0" > /persist/mfg/inproduction
chmod 644 -R /persist/mfg

mkdir -p /persist/wifi
cp /nvram/boardid/macaddresswlan /persist/wifi/.macaddr
chmod 644 -R /persist/wifi

umount /nvram/legacy

start efs_convert
