all clean:
	@$(MAKE) -C src $@
	@$(MAKE) -C examples $@

.PHONY: all clean
