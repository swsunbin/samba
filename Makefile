#
# **************************************************************                *
# *                                                            *
# * Author: sunbin (2023)                                      *
# * URL: https://github.com/samba-team/samba				   *
# *                                                            *
# * Copyright notice:                                          *
# * Free use of this C++ Makefile template is permitted under  *
# * the guidelines and in accordance with the the MIT License  *
# * http://www.opensource.org/licenses/MIT                     *
# *                                                            *
# **************************************************************
#

TOPDIR := $(shell /bin/pwd)
samba_src_dir = $(TOPDIR)
build_dir = $(TOPDIR)/build
samba_dir_name = samba-`cat $(samba_src_dir)/samba.spec | grep "%global samba_version" | awk '{print $$3}'`

all:  .build_samba
  
.build_samba:
	@(if [ -d $(build_dir) ]; then rm -rf $(build_dir); fi)
	@(mkdir -p $(build_dir))
	@(mkdir -p $(build_dir)/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS})
	@echo "---------- copy samba files ----------"
	@(cd $(build_dir)/SOURCES; \
		cp -a $(samba_src_dir)/$(samba_dir_name) $(samba_dir_name); \
		tar cvf $(samba_dir_name).tar $(samba_dir_name); \
		xz -T0 $(samba_dir_name).tar; \
		cp -a $(samba_src_dir)/configfile/* ./; \
		rm -rf $(samba_dir_name))

	@(cp -af $(samba_src_dir)/samba.spec $(build_dir)/SPECS/)

	@echo "---------- build samba ----------"
	@(rpmbuild -ba --define="_topdir $(build_dir)" $(build_dir)/SPECS/samba.spec)

install:
	@(cd $(build_dir)/RPMS/noarch; rpm -vih *.noarch.rpm --force)
	@(cd $(build_dir)/RPMS/x86_64; rpm -vih *.x86_64.rpm --force)
clean:
	-rm -rf $(build_dir)
