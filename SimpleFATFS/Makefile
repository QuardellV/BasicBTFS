obj-m += simplefatfs.o
simplefatfs-objs = fs.o

KDIR ?= /lib/modules/$(shell uname -r)/build

MKFS =  test.simplefatfs

all: $(MKFS)
	make -C $(KDIR) M=$(PWD) modules

IMAGE ?= test.img
IMAGESIZE ?= 200

$(MKFS): mkfs.c
	$(CC) -std=gnu99 -Wall -o $@ $<

$(IMAGE): $(MKFS)
		dd bs=4096 count=${IMAGESIZE} if=/dev/zero of="${IMAGE}"
		./$< $(IMAGE)


clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -f $(MKFS) $(IMAGE)
