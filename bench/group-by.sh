#!/bin/sh
db=/data/local/nakatani/sqlite3/bench_db//1000000records.sqlite3
query="select avg(val_col) from large_table group by key_col;"
. $(cd $(dirname $0);pwd)/common.sh
