#!/bin/sh

./200_latency/ART_LP_I6_sparse_16M_200 > ./result/200_latency/ART_LP_I6_sparse_16M_200.txt
./200_latency/ART_LP_I6_dense_16M_200 > ./result/200_latency/ART_LP_I6_dense_16M_200.txt
./200_latency/ART_LP_I6_synthetic_16M_200 > ./result/200_latency/ART_LP_I6_synthetic_16M_200.txt

./200_latency/ART_LP_I6_sparse_128M_200 > ./result/200_latency/ART_LP_I6_sparse_128M_200.txt
./200_latency/ART_LP_I6_dense_128M_200 > ./result/200_latency/ART_LP_I6_dense_128M_200.txt
./200_latency/ART_LP_I6_synthetic_128M_200 > ./result/200_latency/ART_LP_I6_synthetic_128M_200.txt

./200_latency/ART_LP_I6_sparse_1024M_200 > ./result/200_latency/ART_LP_I6_sparse_1024M_200.txt
./200_latency/ART_LP_I6_dense_1024M_200 > ./result/200_latency/ART_LP_I6_dense_1024M_200.txt
./200_latency/ART_LP_I6_synthetic_1024M_200 > ./result/200_latency/ART_LP_I6_synthetic_1024M_200.txt
