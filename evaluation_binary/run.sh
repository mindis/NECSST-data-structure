#!/bin/sh

./FPTree_sparse_16M > ./result/FPTree_sparse_16M.txt
./FPTree_dense_16M > ./result/FPTree_dense_16M.txt
./FPTree_synthetic_16M > ./result/FPTree_synthetic_16M.txt

./FPTree_sparse_128M > ./result/FPTree_sparse_128M.txt
./FPTree_dense_128M > ./result/FPTree_dense_128M.txt
./FPTree_synthetic_128M > ./result/FPTree_synthetic_128M.txt

./FPTree_sparse_1024M > ./result/FPTree_sparse_1024M.txt
./FPTree_dense_1024M > ./result/FPTree_dense_1024M.txt
./FPTree_synthetic_1024M > ./result/FPTree_synthetic_1024M.txt
