.PHONY: clean

NANOPB_DIR = /home/CORPUSERS/28852506/src/grideye-wifi/.pio/libdeps/release/Nanopb_ID431
CFLAGS = -D PB_FIELD_16BIT
CFLAGS += -I $(NANOPB_DIR)

#CSRC = simple.c
CSRC = $(NANOPB_DIR)/pb_encode.c  # The nanopb encoder
CSRC += $(NANOPB_DIR)/pb_decode.c  # The nanopb decoder
CSRC += $(NANOPB_DIR)/pb_common.c  # The nanopb common parts

simple: simple.c thermalcam.pb.c thermalcam.pb.h
	gcc $(CFLAGS) -o $@ $(CSRC) $^

clean:
	rm -f simple
