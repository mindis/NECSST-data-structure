	mid = Search(curr, curr->slot, key);

	for (j = curr->slot[0]; j > mid; j--)
		temp[j - 1] = curr->slot[j];

	for (j = mid - 1; j >= 1; j--)
		temp[j] = curr->slot[j];

	temp[0] = curr->slot[0] - 1;

	*((uint64_t *)curr->slot) = *((uint64_t *)temp);
	flush_buffer(curr->slot, 8, true);

	return 0;
}

int Delete(tree *t, unsigned long key)
{
	int numEntries, errval = 0;
	node *curr = t->root;
	/* Find proper leaf */
	curr = find_leaf_node(curr, key);

	/* Check underflow & merge */
//	numEntries = curr->slot[0];
//	if(numEntries <= 1) {
//		delete_parent_entry(curr->parent, key);
//	} else
//		errval = delete_in_leaf(curr, key);

	errval = delete_in_leaf(curr, key);
	return errval;
}
