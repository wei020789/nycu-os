.PHONY: clean load unload check
obj-m += kfetch_mod_313553053.o

PWD := $(CURDIR)

all:
	sudo make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

load:
	sudo insmod kfetch_mod_313553053.ko

unload:
	sudo rmmod kfetch_mod_313553053

clean:
	sudo make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

# check:
# 	sudo ./kfetch  && free -m
# 	sudo ./kfetch  && ps -e | wc -l
# 	sudo hostnamectl set-hostname cwkang-VMware-Virtual-Platform7777777
# 	sudo ./kfetch
# 	sudo hostnamectl set-hostname cwkang-VMware-Virtual-Platform
# 	sudo ./kfetch