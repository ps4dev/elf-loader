.DEFAULT_GOAL := all

%:
	$(MAKE) -C generate $@ $?
	$(MAKE) -C ps4/library $@ $?
	$(MAKE) -C ps4/binary/user $@ $?
	$(MAKE) -C ps4/binary/kernel $@ $?
