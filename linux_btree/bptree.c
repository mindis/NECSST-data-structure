#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/btree.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Se Kwon Lee");
struct btree_head bphead;

static int __init init_bptree(void) {
	printk(KERN_ALERT "B+tree start!!\n");
//	struct btree_head *head;
	unsigned long key1 = 1, key2 = 2, key3 = 3;
	unsigned long var1 = 10, var2 = 20, var3 = 30;
	unsigned long *key1_var, *key2_var, *key3_var;
	int errval;
//	key1_var = &key1, key2_var = &key2, key3_var = &key3;
	errval = btree_init(&bphead);
	if(errval != 0) {
		printk(KERN_ALERT "init error\n");
		return 0;
	}
	
	errval = btree_insert(&bphead, &btree_geo64, &key1, &var1, GFP_KERNEL);
	if(errval != 0) {
		printk(KERN_ALERT "insert error 1\n");
		return 0;
	}
	errval = btree_insert(&bphead, &btree_geo64, &key2, &var2, GFP_KERNEL);
	if(errval != 0) {
		printk(KERN_ALERT "insert error 2\n");
		return 0;
	}
	errval = btree_insert(&bphead, &btree_geo64, &key3, &var3, GFP_KERNEL);
	if(errval != 0) {
		printk(KERN_ALERT "insert error 3\n");
		return 0;
	}

	key1_var = btree_lookup(&bphead, &btree_geo64, &key1);
	key2_var = btree_lookup(&bphead, &btree_geo64, &key2);
	key3_var = btree_lookup(&bphead, &btree_geo64, &key3);

	printk(KERN_ALERT "key1_var = %lu\n", *key1_var);
	printk(KERN_ALERT "key1_var = %lu\n", *key2_var);
	printk(KERN_ALERT "key1_var = %lu\n", *key3_var);

	return 0;
}

static void __exit exit_bptree(void) {
	printk(KERN_ALERT "B+tree end!!!\n");
}

module_init(init_bptree);
module_exit(exit_bptree);
