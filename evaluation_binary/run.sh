#!/bin/sh

./200_latency/wbtree_sparse_16M_200 > ./result/200_latency/wbtree_sparse_16M_200.txt
./200_latency/wbtree_dense_16M_200 > ./result/200_latency/wbtree_dense_16M_200.txt
./200_latency/wbtree_synthetic_16M_200 > ./result/200_latency/wbtree_synthetic_16M_200.txt

./200_latency/wbtree_sparse_128M_200 > ./result/200_latency/wbtree_sparse_128M_200.txt
./200_latency/wbtree_dense_128M_200 > ./result/200_latency/wbtree_dense_128M_200.txt
./200_latency/wbtree_synthetic_128M_200 > ./result/200_latency/wbtree_synthetic_128M_200.txt

./200_latency/wbtree_sparse_1024M_200 > ./result/200_latency/wbtree_sparse_1024M_200.txt
./200_latency/wbtree_dense_1024M_200 > ./result/200_latency/wbtree_dense_1024M_200.txt
./200_latency/wbtree_synthetic_1024M_200 > ./result/200_latency/wbtree_synthetic_1024M_200.txt
