
.PHONY:

CORE?="hifi4"
XA_PRIORITY ?= 0

all all-dec: host-lib
	$(MAKE) -f makefile_testbench XF_CORE_ID=0 XAF=host XA_RTOS=linux $@

host-lib: 
	$(MAKE) -f makefile_host all install

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
