KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all: module user_app

module:
	$(MAKE) -C $(KDIR) M=$(PWD)/src modules

user_app:
	gcc -Wall -Wextra -O2 user/test_app.c -o user/test_app

clean:
	$(MAKE) -C $(KDIR) M=$(PWD)/src clean
	rm -f user/test_app
