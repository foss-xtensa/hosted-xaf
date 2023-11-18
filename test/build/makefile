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

CORE_LIST2=$(shell seq 0 1 $(NCORES))
CORE_LIST=$(filter-out $(NCORES),$(CORE_LIST2))

ifeq ($(CACHE_BYPASS), 0)
CACHE_ENABLED=1
else
CACHE_ENABLED=0
endif
XA_ZERO_COPY?=0

all all-dec af_hostless:
	$(foreach var, $(CORE_LIST),\
	$(info "core:${var}";)\
	$(MAKE) -f makefile_testbench_dsp clean XTCORE="core$(var)";\
    $(MAKE) -f makefile_testbench_dsp $@ XF_LOCAL_IPC_NON_COHERENT=$(CACHE_ENABLED) XF_CFG_CORES_NUM=$(NCORES) XTCORE="core$(var)" XAF_HOSTED_DSP=$(XAF_HOSTED_DSP) XA_ZERO_COPY=$(XA_ZERO_COPY) XF_CORE_ID=${var};\
	)

clean:
	@$(foreach var, $(CORE_LIST),\
	$(info "core:${var}";)\
	$(MAKE) -f makefile_testbench_dsp clean XTCORE="core$(var)";\
	)

