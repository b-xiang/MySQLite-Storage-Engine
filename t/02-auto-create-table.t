#! /usr/bin/perl

use strict;
use warnings;

use DBI;

use Test::More tests => 17;

use File::Basename;
use Cwd 'realpath';
my $testdir = realpath(dirname(__FILE__));

my $dbh = DBI->connect(
    $ENV{DBI} || 'dbi:mysql:database=test;host=localhost',
    $ENV{DBI_USER} || 'root',
    $ENV{DBI_PASSWORD} || '',
) or die 'connection failed:';


# 1 table with short DDL
ok($dbh->do("drop table if exists test02_table1_ddl_short_t1"));
ok($dbh->do("select sqlite_db('$testdir/db/02-table1-ddl_short.sqlite')"));
is_deeply(
    $dbh->selectall_arrayref("select column_name from information_schema.columns where table_name='test02_table1_ddl_short_t1'"),
    [ ['c1'] ],
);


# 3 tables w/ short schema
ok($dbh->do("drop table if exists test02_table3_ddl_short_t1"));
ok($dbh->do("drop table if exists test02_table3_ddl_short_t2"));
ok($dbh->do("drop table if exists test02_table3_ddl_short_t3"));
ok($dbh->do("select sqlite_db('$testdir/db/02-table3-ddl_short.sqlite')"));
is_deeply(
    $dbh->selectall_arrayref("select column_name from information_schema.columns where table_name='test02_table3_ddl_short_t1'"),
    [ ['c1'] ],
);
is_deeply(
    $dbh->selectall_arrayref("select column_name from information_schema.columns where table_name='test02_table3_ddl_short_t2'"),
    [ ['cc1'] ],
);
is_deeply(
    $dbh->selectall_arrayref("select column_name from information_schema.columns where table_name='test02_table3_ddl_short_t3'"),
    [ ['ccc1'] ],
);


# 1 table w/ so long schema (that it exeeds page#1 of SQLite DB)
ok($dbh->do("drop table if exists test02_table1_ddl_long_t1"));
ok($dbh->do("select sqlite_db('$testdir/db/02-table1-ddl_long.sqlite')"));
is_deeply(
    $dbh->selectall_arrayref("select column_name from information_schema.columns where table_name='test02_table1_ddl_long_t20'"),
    [ ['col20'] ],
);


# Tables should be created on current DB
TODO: {
    local $TODO = 'BUG: mysql_real_connect hard coding';
    ok($dbh->do("drop database if exists test02db_for_mysqlite"));
    ok($dbh->do("create database test02db_for_mysqlite"));
    my $host = $dbh->private_data->{host};
    my $database = $dbh->private_data->{database};
    my $dbh2 = DBI->connect(
        'dbi:mysql:database=' . $database . ';host=' . $host,
        $ENV{DBI_USER} || 'root',
        $ENV{DBI_PASSWORD} || '',
    ) or die 'connection failed:';
    ok($dbh2->do("select sqlite_db('$testdir/db/02-table1-ddl_short.sqlite')"));
    is_deeply(
        $dbh2->selectall_arrayref("select count(*) from test02db_for_mysqlite.test02_table1_ddl_short_t1"),
        [ [0] ],
    );
}
