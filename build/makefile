#
# Copyright (c) 2015-2023 Cadence Design Systems, Inc.
# 
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
# 
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#


.PHONY:

CORE?="hifi4"
NCORES ?= 1
CACHE_BYPASS?=0
ROOTDIR ?= ..
XAF_HOSTED_DSP?=1
PLUGIN_DIR=$(ROOTDIR)/test/plugins/cadence

CORE_LIST2=$(shell seq 0 1 $(NCORES))
CORE_LIST=$(filter-out $(NCORES),$(CORE_LIST2))

ifeq ($(CACHE_BYPASS), 0)
CACHE_ENABLED=1
else
CACHE_ENABLED=0
endif

sysbuild:
	$(MAKE) -C $(ROOTDIR)/xtsc NCORES=$(NCORES) clean all


XTCORE="core0"
XF_CORE_ID=0
all all-dec af_hostless:
	$(MAKE) -f makefile_lib clean XTCORE=$(XTCORE)
	$(MAKE) -f makefile_lib $@ XF_LOCAL_IPC_NON_COHERENT=$(CACHE_ENABLED) XF_CFG_CORES_NUM=$(NCORES) XTCORE=$(XTCORE) XF_CORE_ID=${XF_CORE_ID} XAF_HOSTED_DSP=$(XAF_HOSTED_DSP)

install:
	$(MAKE) -f makefile_lib $@ XF_LOCAL_IPC_NON_COHERENT=$(CACHE_ENABLED) XF_CFG_CORES_NUM=$(NCORES) XTCORE=$(XTCORE) XF_CORE_ID=${XF_CORE_ID} XAF_HOSTED_DSP=$(XAF_HOSTED_DSP)

clean:
	$(MAKE) -f makefile_lib clean XTCORE=$(XTCORE)

pack_all: sysbuild all

