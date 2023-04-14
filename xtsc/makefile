
.PHONY:

NCORES ?= 1
ROOTDIR ?= ..
SHMEM_ADDR ?= 0xE0000000
SHMEM_OFFSET = 0x1000
xf_cfg_shmem_addr=$(shell printf "0x%x\n" $(shell echo $$(( $(SHMEM_OFFSET) + $(SHMEM_ADDR) ))))
XTENSA_TOOLS=`xt-run --show-config=xttools`
CP = cp -f

YML_FILE=$(ROOTDIR)/xtsc/xaf_xtsc_sys_1c.yml
XTSYS_FILE=$(ROOTDIR)/xtsc/xaf_xtsc_sys_1c.xtsys

all sysbuild:
	sed -i "/{Name\s*:\s*Shared_RAM_L,\.*/{n; s/\(GlobalAddr\s*:\s*\)\(0x.*\)/\1$(SHMEM_ADDR),/;}" xaf_xtsc_sys_1c.yml
	$(XTENSA_TOOLS)/libexec/xt-mbuild -root ./ -socfile $(XTSYS_FILE) -syspkg $(ROOTDIR)/xtsc/mbuild/package -sysbuild $(ROOTDIR)/xtsc/mbuild/build -preclean -build sw -lsps sim,sim-local
	$(XTENSA_TOOLS)/libexec/xt-sysbuilder -subsys $(YML_FILE) -swtools $(XTENSA_TOOLS) -build $(ROOTDIR)/xtsc/sysbuilder --xtensa-system $(XTENSA_SYSTEM) 
	sed -i "s/#define MEM_START\(\s\+\).*/#define MEM_START\1$(SHMEM_ADDR)/" $(ROOTDIR)/algo/hifi-dpf/include/sys/xos-msgq/xf-dp_ipc.h
	sed -i "s/#define XF_CFG_SHMEM_ADDRESS(core)\(\s\+\).*/#define XF_CFG_SHMEM_ADDRESS(core)\1$(xf_cfg_shmem_addr)/g" $(ROOTDIR)/algo/hifi-dpf/include/sys/xos-msgq/iss/xf-board.h
	sed -i "s/#define XF_CFG_SHMEM_ADDRESS(core)\(\s\+\).*/#define XF_CFG_SHMEM_ADDRESS(core)\1$(xf_cfg_shmem_addr)/" $(ROOTDIR)/include/xaf-api.h
	sed -i "s/#define XF_PROXY_DATA_ADDRESS(core)\(\s\+\).*/#define XF_PROXY_DATA_ADDRESS(core)\1$(SHMEM_ADDR)/" $(ROOTDIR)/xf_kernel_driver/include/sys/xt-shmem/xf-config.h
	sed -i "/\$$(core0_SIM_CLIENT_FILE)/a --set_core_parm=SimTargetOutput=\$$(core0_RUN_LOG)" $(ROOTDIR)/xtsc/sysbuilder/xtsc-run/core0.inc
	sed -i "s/#define INTR_SHMEM\(\s\+\).*/#define INTR_SHMEM\1$(shell printf "0x%x\n" $(shell echo $$(( $(SHMEM_ADDR) + 0x184))))/" TestBenchMaster1c.vec
	sed -i "s/#define CMD_ADDR\(\s\+\).*/#define CMD_ADDR\1$(shell printf "0x%x\n" $(shell echo $$(( $(SHMEM_ADDR) + 0x104))))/" TestBenchMaster1c.vec
	$(CP) TestBenchMaster1c.vec $(ROOTDIR)/xtsc/sysbuilder/xtsc-run/TestBenchMaster.vec
	sed -i "s/^SUBSYS_DIR=.*/SUBSYS_DIR=\"..\/..\/xtsc\"/" $(ROOTDIR)/test/build/cosim-launcher2.py
	sed -i "s/^SHMEM_ADDR=.*/SHMEM_ADDR=$(SHMEM_ADDR)/" $(ROOTDIR)/test/build/cosim-launcher2.py

clean:
	rm -rf $(ROOTDIR)/xtsc/mbuild $(ROOTDIR)/xtsc/sysbuilder
