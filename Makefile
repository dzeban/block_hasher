default: block_hasher

CC=gcc
CC_OPTS= -lrt -pthread -lcrypto

block_hasher: block_hasher.c
	$(CC) $^ $(CC_OPTS) -o $@
