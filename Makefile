#----------------------------------------------------------
#
# Lab #2 : NAND Simulator
#  - Embedded Systems Design, ICE3028 (Fall, 2022)
#
# Sep. 23, 2022.
#
# TA: Jinwoo Jeong, Jeeyoon Jung
# Prof: Dongkun Shin
# Embedded Software Laboratory
# Sungkyunkwan University
# http://nyx.skku.ac.kr
#
#----------------------------------------------------------

STUDENT_ID = 2021000000

CC	= gcc
CFLAGS	= -g -O2 -Wall -std=c99
RM	= rm
TAR	= tar

TARGET	= ftl_test
SRCS	= ftl_test.c ftl.c nand.c
HEADERS	= nand.h ftl3.h
OBJS	= $(SRCS:.c=.o)

all: $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

submit:
	$(RM) -f $(STUDENT_ID).tar.gz
	$(TAR) cvzf $(STUDENT_ID).tar.gz ftl.c
	ls -l $(STUDENT_ID).tar.gz

clean:
	$(RM) -f $(TARGET) $(STUDENT_ID).tar.gz $(OBJS)
