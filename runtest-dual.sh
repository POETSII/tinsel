#!/bin/bash
# HOST=sappho
# TINSELPATH=/home/cmw97/tinsel
# ADDR=0x60140f00000

HOST=betsy

if [ $HOST = "ASDEX" ]; then
  TINSELPATH=/mnt/Projects/POETS/tinsel
  ADDR=a2301000
  SSH=ssh
  envs="QUARTUS_ROOTDIR=/local/ecad/altera/19.2pro/quartus PATH=\$QUARTUS_ROOTDIR/bin:\$QUARTUS_ROOTDIR/../qsys/bin:\$QUARTUS_ROOTDIR/../nios2eds/bin:\$QUARTUS_ROOTDIR/../nios2eds/sdk2/bin:\$QUARTUS_ROOTDIR/../nios2eds/bin/gnu/H-x86_64-pc-linux-gnu/bin:\$PATH"
fi

if [ $HOST = "betsy" ]; then
  TINSELPATH=`pwd`
  SSH="ssh -K"
  envs="QUARTUS_ROOTDIR=/usr/groups/ecad/altera/19.2pro/quartus PATH=\$QUARTUS_ROOTDIR/bin:\$QUARTUS_ROOTDIR/../qsys/bin:\$QUARTUS_ROOTDIR/../nios2eds/bin:\$QUARTUS_ROOTDIR/../nios2eds/sdk2/bin:\$QUARTUS_ROOTDIR/../nios2eds/bin/gnu/H-x86_64-pc-linux-gnu/bin:\$PATH LD_LIBRARY_PATH=/usr/groups/ecad/altera/19.2pro/quartus/dspba/backend/linux64/syscon:/usr/groups/ecad/altera/19.2pro/quartus/linux64/"
  ADDR=f7001000
fi



MAGIC_ADDR_DEC=$(( 0x$ADDR + 0xC0 ))
MAGIC_ADDR=`printf \"%x\" $MAGIC_ADDR_DEC`

trap "trap - SIGTERM && kill -- -$$" SIGINT SIGTERM EXIT
set -e

make -C hostlink pciestreamd boardctrld hostlink.a echo
make -C apps/hello all run

# make -C hostlink-custom pciestreamd boardctrld POST
BOOT_UP_TO_DATE=$( make -C apps/boot all | tee /dev/tty )
echo $BOOT_UP_TO_DATE


if  [ $( echo "${BOOT_UP_TO_DATE}" | grep -c "Nothing to be done for 'all'" ) == "0" ] ; then
  echo "updating bootloader in .sof"
  make -C de10-pro update-mif
else
  echo "bootloader up-to-date; not rebuilding mif"
fi


if [ $HOST = "ASDEX" ]; then
  $SSH $HOST sudo mount -t cifs -o user=tparks,pass=2447pass,uid=1000,gid=1000 //192.168.0.116/culham /mnt || true
fi

if [ $HOST = "sappho" ]; then
  unison -batch=true . ssh://sappho//home/cmw97/tinsel
fi


$SSH $HOST sudo -S pkill boardctrld || true # make sure we have access to reflash
$SSH $HOST sudo -S pkill echo || true # make sure we have access to reflash

if [ $HOST = "ASDEX" ]; then
  # ssh $HOST $envs quartus_pgm -m jtag -o "\"p;$TINSELPATH/DE10Pro/DE10-reference-project/build/top.sof@1\""
  $SSH $HOST $envs /home/coral/.local/bin/flashDE10 $TINSELPATH/de10-pro/output_files/DE10_Pro.sof
fi

if [ $HOST = "sappho" ]; then
  $SSH $HOST $envs /home/cmw97/tools/bin/flashDE10 $TINSELPATH/de10-pro/output_files/DE10_Pro.sof
fi

if [ $HOST = "betsy" ]; then
  $SSH $HOST $envs /home/cmw97/tools/bin/flashDE10 $TINSELPATH/de10-pro/output_files/DE10_Pro.sof --all
fi

# ssh $HOST $envs env
$SSH $HOST sudo reboot || true

set +e
until ssh -o ConnectTimeout=1 $HOST echo 1; do
    sleep 1
done
set -e

if [ $HOST = "ASDEX" ]; then
  $SSH $HOST sudo mount -t cifs -o user=tparks,pass=2447pass,uid=1000,gid=1000 //192.168.0.116/culham /mnt || true
fi

if [ $HOST = "sappho" ]; then
  unison -auto . ssh://sappho//home/cmw97/tinsel
fi

$SSH $HOST sudo -S rmmod altera_cvp || true
$SSH $HOST sudo -S setpci -v -d 1172:de10 COMMAND=0x06:0x06
$SSH $HOST sudo -S lspci -d 1172:de10 -vvv -nn
$SSH $HOST "cd $TINSELPATH/hostlink/driver && make all" || true
$SSH $HOST sudo -S insmod $TINSELPATH/hostlink/driver/dmabuffer.ko || true
$SSH $HOST sudo -S devmem2 0x$MAGIC_ADDR
# ssh $HOST "sudo $TINSELPATH/hostlink/echo"

$SSH $HOST "sudo -S $TINSELPATH/hostlink/pciestreamd $ADDR > pcielog.txt" &
$SSH $HOST $envs $TINSELPATH/hostlink/boardctrld &
ssh -X $HOST $envs quartus_stpw $TINSELPATH/DE10Pro/DE10-reference-project/quartus/stp1.stp &

# # sleep 5
# ssh -X $HOST "cd $TINSELPATH/apps/hello && ./run"
$SSH -X $HOST "cd $TINSELPATH/apps/hello && urxvt"

# # sleep 5
# ssh -X $HOST "cd $TINSELPATH/apps/hello && ./run"
#  -e bash -c \"./run | tee log.txt && bash\""
# ssh $HOST "sudo $TINSELPATH/hostlink/echo"
