.DEFAULT_GOAL := tests
.PHONY: udt4 tests

udt4:
	$(MAKE) -C udt4 all

tests: udt4
	$(MAKE) -C tests all

clean:
	$(MAKE) -C udt4 clean
	$(MAKE) -C tests clean
