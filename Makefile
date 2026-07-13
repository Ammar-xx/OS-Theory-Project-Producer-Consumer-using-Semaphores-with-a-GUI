CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -pthread
LDFLAGS = -pthread
LDLIBS  = -lm

all: pc_sem benchmark

pc_sem: main.o buffer.o producer.o consumer.o
	$(CC) $(LDFLAGS) -o $@ $^

benchmark: benchmark.o buffer.o monitor.o fair_buffer.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

main.o:        main.c        buffer.h producer.h consumer.h
buffer.o:      buffer.c      buffer.h
producer.o:    producer.c    producer.h buffer.h
consumer.o:    consumer.c    consumer.h buffer.h
monitor.o:     monitor.c     monitor.h buffer.h
fair_buffer.o: fair_buffer.c fair_buffer.h buffer.h
benchmark.o:   benchmark.c   buffer.h monitor.h fair_buffer.h

clean:
	rm -f *.o pc_sem benchmark results.csv

run: pc_sem
	./pc_sem

bench: benchmark
	./benchmark > results.csv
	@echo "Results written to results.csv"

test: pc_sem
	@bash tests/run_tests.sh

.PHONY: all clean run bench test
