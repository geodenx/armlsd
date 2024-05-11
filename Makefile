CFLAGS_DEFAULT=-Wall -O

armlsd: armlsd.c
	${CC} ${CFLAGS} ${CFLAGS_DEFAULT} -o $@ $@.c

clean:
	-rm armlsd
