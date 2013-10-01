all clean:
	@$(MAKE) -C src $@

examples:
	@$(MAKE) -C examples $@

.PHONY: all clean examples
