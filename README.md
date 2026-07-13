# Producer-Consumer Using Semaphores
----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
OS Course Project — Spring 2026, Project 15.

A multi-threaded producer-consumer simulation in C using POSIX semaphores.

## Build

```
make
```

## Run

```
./pc_sem                            # defaults: 2P, 2C, buf=5, 10 items each
./pc_sem -p 4 -c 2 -b 8 -i 20       # custom config
./pc_sem -h                         # help
make test                           # run test suite
```

## Team

- Muhammad Ammmar — Phase 1 (Core Engine)
- Ali Hussain — Phase 2 (Monitor / Performance)
- Huzaifa Shahid — Phase 3 (Logger / Dashboard)
