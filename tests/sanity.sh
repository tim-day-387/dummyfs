#!/bin/bash
set -e

KMOD_NAME=dummyfs
ROOT_DIR=$PWD
KMOD_LOC=$ROOT_DIR/dummyfs.ko
MKFS_LOC=$ROOT_DIR/utils/mkfs.dummyfs
TRUN_LOC=$ROOT_DIR/utils/truncate


install_kmod() {
  sudo insmod $KMOD_LOC
  lsmod | grep $KMOD_NAME
}


mk_clean_fs() {
  dd if=/dev/zero of=test.img bs=512 count=100
  $MKFS_LOC test.img | tail -n 3
}


mk_dir_and_mount() {
  mkdir testmountpoint
  sudo mount -o loop -t $KMOD_NAME test.img testmountpoint

  mkdir debugmountpoint
  sudo mount -t dumdbfs none debugmountpoint

  cd testmountpoint
}


write_read_files() {
  echo "hello" > file1
  ls
  echo "by" > file2
  ls
  echo "help" > file3
  ls
  cat file1 file2 file3
  rm file2
  ls
  rm file3
  ls
  rm file1
  ls
}


test_dumdbfs() {
  echo "start - test dumdbfs"
  cat $ROOT_DIR/debugmountpoint/counter
  cat $ROOT_DIR/debugmountpoint/counter
  cat $ROOT_DIR/debugmountpoint/counter
  echo 0 | sudo tee $ROOT_DIR/debugmountpoint/counter
  cat $ROOT_DIR/debugmountpoint/counter
  cat $ROOT_DIR/debugmountpoint/counter
  cat $ROOT_DIR/debugmountpoint/counter
  echo "end - test dumdbfs"
}


truncate_files() {
  echo "short-long" > file1
  cat file1
  $TRUN_LOC file1 5
  cat file1
  $TRUN_LOC file2 3
  cat file2
  echo "a" > file3
  cat file3
  $TRUN_LOC file3 4
  cat file3
  rm file1
  rm file2 
  rm file3
  ls
}


umount_dir() {
  cd $ROOT_DIR
  sudo umount testmountpoint
  sudo umount debugmountpoint
}


remove_kmod() {
  sudo rmmod $KMOD_NAME
}


clean() {
  rm test.img
  rm -rf testmountpoint
  rm -rf debugmountpoint
}


echo "dummyfs sanity tests - start"

if [[ $1 == "clean" ]]
then
  umount_dir
  remove_kmod
  clean
else
  install_kmod
  mk_clean_fs
  mk_dir_and_mount
  write_read_files
  test_dumdbfs
  umount_dir
  remove_kmod
  clean
fi

echo "dummyfs sanity tests - end"
