


void Lookup(Key K)
{
	Leaf = FindLeaf(K);

	for (each slot in Leaf) {
		currentKey = Leaf.KV[slot].PKey;
		if (Leaf.Bitmap[slot] == 1 && Leaf.Fingerprints[slot] == hash(K)
				&& currentKey == K) {
			val = Leaf.KV[slot].Val;
			break;
		}
	}
	return val;
}

void Insert(Key K, Value V)
{
	(Leaf, Parent) = FindLeaf(K);
	Decision = Leaf.isFull() ? Result::Split : Result::Insert;

	if (Decision == Result::Split)
		splitKey = SplitLeaf(Leaf);
	
	slot = Leaf.Bitmap.FindFirstZero();
	Leaf.KV[slot] = (K, V);
	Leaf.Fingerprints[slot] = hash(K);
	Persist(Leaf.KV[slot]);
	Persist(Leaf.Fingerprints[slot]);
	Leaf.Bitmap[slot] = 1;
	Persist(Leaf.Bitmap);

	if (Decision == Result::Split)
		UpdateParents(splitKey, Parent, Leaf);
}
