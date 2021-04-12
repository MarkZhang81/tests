# tests
** Misc test programs **

* fl_udp_sport_hash
Test the hash distribution for the fl/udp_sport feature. Ref.: https://www.spinics.net/lists/linux-rdma/msg87626.html
```
Usage:
    $ gcc -Wall -o u  hashtest_udp_sport.c
    $ ./u 0 1
    $ (ssh -X <server>)
    $ gnuplot -p hashout.gnuplot
