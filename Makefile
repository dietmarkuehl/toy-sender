#  Makefile                                                      -*-makefile-*-
#  SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#  ----------------------------------------------------------------------------

.PHONY: default configure clean

default: configure
	@cmake --build build

configure:
	@cmake -S . -B build

clean:
	@rm -rf build
	@$(RM) mkerr olderr
