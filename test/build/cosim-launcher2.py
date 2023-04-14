#!/usr/bin/python3

import argparse
import ast
import collections
import os, signal
import shutil
import sys
import shlex, subprocess
import time

QEMU_BASE_DIR="/servers/scratch50g/nddivya/projects/dev/hf2/tie/framework/hosted/linux_images/32bit"
SSH_FORWARD_PORT = 10022
SUBSYS_DIR="."
LOG_FILE="c0_run_log.txt"
SHMEM_ADDR=0xE0000000

QEMU_BINARY_PATH    = QEMU_BASE_DIR+"/qemu-system-x86_64"
BIOS_BINARY_PATH    = QEMU_BASE_DIR+"/pc-bios"
UBUNTU_IMG_PATH     = QEMU_BASE_DIR+"/xenial-server-cloudimg-i386-disk1.img"
USER_DATA_IMG_PATH  = QEMU_BASE_DIR+"/user-data.img"

def get_env_variable(env):
  try: 
    var = os.getenv(env)
  except OSError:
    pass
  return var

def xtsc_cmd(xtensa_core, xtensa_system, binary, pid, xtsc_ca):
  if xtsc_ca == True:
    xtsc_turbo = "false"
  else:
    xtsc_turbo = "true"
  cmd = "xtsc-run " \
      + " --xtensa-system=" + xtensa_system \
      + " --set_xtsc_parm=turbo=" + xtsc_turbo \
      + " --define=core0_BINARY=" + binary  \
      + " --define=core0_RUN_LOG=" + LOG_FILE \
      + " --define=SHARED_RAM_L_NAME=SharedRAM_L."+str(pid) \
      + " --define=SYSTEM_RAM_L_NAME=SystemRAM_L."+str(pid) \
      + " --define=SYSTEMRAM_DELAY=100" \
      + " --define=SYSTEMROM_DELAY=100" \
      + " --define=SYSTEM_RAM_L_DELAY=100" \
      + " --define=SHARED_RAM_L_DELAY=100" \
      + " --include=" + SUBSYS_DIR + "/sysbuilder/xtsc-run/multicore1c.inc"
  return cmd

def main(SHARED_MEM_ADDR):
  pid = os.getpid()
  os.makedirs("./profile", exist_ok=True)
  os.putenv('XTSC_PID', str(pid))
  xtensa_system = get_env_variable('XTENSA_SYSTEM')
  xtensa_core = get_env_variable('XTENSA_CORE')
  binary = os.getcwd()+"/xa_af_hosted_dsp_test_core0"
  cmd = xtsc_cmd(xtensa_core, xtensa_system, binary, pid, 1)    
  print(cmd)
  args = shlex.split(cmd)
  xtsc = subprocess.Popen(args)
  time.sleep(3)
  qemu_cmd = f"{QEMU_BINARY_PATH} -L {BIOS_BINARY_PATH} -smp 2 -m 1024 -serial file:serial.log -display none --netdev user,id=net0,hostfwd=tcp::{SSH_FORWARD_PORT:d}-:22 -device e1000,netdev=net0 -drive if=virtio,file={UBUNTU_IMG_PATH},cache=none -drive if=virtio,file={USER_DATA_IMG_PATH},format=raw -device xtensa_xtsc,addr={SHARED_MEM_ADDR},size=0x0e000000,comm_addr={SHARED_MEM_ADDR}"
  args = shlex.split(qemu_cmd)
  print(qemu_cmd)
  try:
    #qemu=subprocess.Popen(args, stdout=None)
    #qemu.wait()
    qemu=subprocess.check_call(args, stdout=None)
  except:
    print("QEMU launch failed");
  xtsc.kill()
  # cleanup
  print("Removing SharedRAM_L."+str(pid)+" SystemRAM_L."+str(pid))
  try:
    os.remove("/dev/shm/SharedRAM_L."+str(pid))
  except:
    pass
  try:
    os.remove("/dev/shm/SystemRAM_L."+str(pid))
  except:
    pass

if __name__ == '__main__':
  if len(sys.argv) < 2:
      print("\nShared Memory Address Not Provided, default address is {}\n" .format(hex(SHMEM_ADDR)))
      print("Usage: {} <addr>".format(sys.argv[0]))
      print("Example: {} {:#x}\n".format(sys.argv[0], 0xC0000000))
      user_input = input("Proceed?[Y/N]:")
      if user_input == 'y' or user_input == 'Y':
          main(SHMEM_ADDR)
      else:
          sys.exit(1)
  else:
      shared_mem_addr = sys.argv[1]
      main(shared_mem_addr)
