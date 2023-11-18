
.PHONY:

CORE?="hifi4"
NCORES?=1
XA_PRIORITY ?= 0
XA_ZERO_COPY ?= 0

af_hostless all all-dec: host-lib
	$(MAKE) -f makefile_testbench XF_CORE_ID=0 XAF=host XA_RTOS=linux XA_ZERO_COPY=$(XA_ZERO_COPY) XF_CFG_CORES_NUM=$(NCORES) $@

host-lib: 
	$(MAKE) -f makefile_host XA_ZERO_COPY=$(XA_ZERO_COPY) XF_CFG_CORES_NUM=$(NCORES) all install

run run-dec:
	$(MAKE) -f makefile_testbench $@

clean_lib:
	$(MAKE) -f makefile_host clean

clean_test:
	$(MAKE) -f makefile_testbench clean

clean clean_all: clean_lib clean_test

h help: 
	@echo "make CORE=[fs_avs|hifi5|hifi4|hifi3] XF_TRACE=1 or 2(default 0) [all-dec|all]"
	@echo "make CORE=[fs_avs|hifi5|hifi4|hifi3] [run-dec|run]"
	@echo "make CORE=[fs_avs|hifi5|hifi4|hifi3] [clean or clean_all|clean_lib|clean_test]"
