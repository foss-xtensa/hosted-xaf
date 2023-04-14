#! /bin/bash

HOSTED_RUN=1
NCORES=1
test_args=
BIN=
XTCORE=core0

# This is required only for standalone "default" regression run.
test_num=$3

echo "usage:<this_script> NCORES <1(def)> <xt-run xa_af_...test -infile:... -outfile:... -C: -D: ...>"
for i in "$@"
do
  case ${i} in
    "xt-run")
        if [ "${BIN}" == "" ];then
            BIN=${2}
        fi
    ;;
    XTCORE)
        XTCORE=${2}
        echo "XTENSA_CORE:$XTCORE"
        shift 2
        ;;
    *)
        arg=${i}
        if [ "${BIN}" != "${i}" ];then
          if [ -n "$test_args" ];then
            if [ $NCORES == 1 ];then
              test_args="${test_args} ${arg}"
            else
              test_args="${test_args},${arg}"
            fi
          else
              test_args="${arg}"
          fi
        fi
    ;;
  esac
done

#remove everything before the 1st -infile regex
test_args=$(echo ${test_args} | awk --re-interval 'match($0,/-infile|-samples|-outfile/){print substr($0,RSTART)}')

# This is required only for standalone "default" regression run.
re='^[0-9]+$'
if [[ $test_num =~ $re ]] ; then test_args=$test_num,$test_args; fi


#export XTSC_TEXTLOGGER_CONFIG_FILE="TextLogger.txt"

RUN="sudo "

cmd="${RUN}\
        ${BIN} \
        ${test_args} \
        "

#reinsert kernel module, workaround for avoiding any stale state in kernel-IPC due to any fatal errors during execution
sudo rmmod xtensa_hifi
sudo insmod ~/kernel_driver/xtensa-hifi.ko

echo ${cmd}
${cmd}

#allowing time for DSP to reboot after the test finishes
sleep 60
