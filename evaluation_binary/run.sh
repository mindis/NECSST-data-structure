#!/bin/sh

./no_latency/PART_sparse_16M_7 > ./result/no_latency/PART_sparse_16M_7.txt
./no_latency/PART_dense_16M_7 > ./result/no_latency/PART_dense_16M_7.txt
./no_latency/PART_synthetic_16M_7 > ./result/no_latency/PART_synthetic_16M_7.txt

./no_latency/PART_sparse_128M_7 > ./result/no_latency/PART_sparse_128M_7.txt
./no_latency/PART_dense_128M_7 > ./result/no_latency/PART_dense_128M_7.txt
./no_latency/PART_synthetic_128M_7 > ./result/no_latency/PART_synthetic_128M_7.txt

./no_latency/PART_sparse_1024M_7 > ./result/no_latency/PART_sparse_1024M_7.txt
./no_latency/PART_dense_1024M_7 > ./result/no_latency/PART_dense_1024M_7.txt
./no_latency/PART_synthetic_1024M_7 > ./result/no_latency/PART_synthetic_1024M_7.txt
