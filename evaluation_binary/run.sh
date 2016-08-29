#!/bin/sh

./400_latency/PART_sparse_16M_8_400 > ./result/400_latency/PART_sparse_16M_8_400.txt
./400_latency/PART_dense_16M_8_400 > ./result/400_latency/PART_dense_16M_8_400.txt
./400_latency/PART_synthetic_16M_8_400 > ./result/400_latency/PART_synthetic_16M_8_400.txt

./400_latency/PART_sparse_128M_8_400 > ./result/400_latency/PART_sparse_128M_8_400.txt
./400_latency/PART_dense_128M_8_400 > ./result/400_latency/PART_dense_128M_8_400.txt
./400_latency/PART_synthetic_128M_8_400 > ./result/400_latency/PART_synthetic_128M_8_400.txt

./400_latency/PART_sparse_1024M_8_400 > ./result/400_latency/PART_sparse_1024M_8_400.txt
./400_latency/PART_dense_1024M_8_400 > ./result/400_latency/PART_dense_1024M_8_400.txt
./400_latency/PART_synthetic_1024M_8_400 > ./result/400_latency/PART_synthetic_1024M_8_400.txt
