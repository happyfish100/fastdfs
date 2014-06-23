#include "avl_tree.h"

int avl_tree_init(AVLTreeInfo *tree, FreeDataFunc free_data_func, \
	CompareFunc compare_func)
{
	tree->root = NULL;
	tree->free_data_func = free_data_func;
	tree->compare_func = compare_func;
	return 0;
}

static void avl_tree_destroy_loop(FreeDataFunc free_data_func, \
		AVLTreeNode *pCurrentNode)
{
	if (pCurrentNode->left != NULL)
	{
		avl_tree_destroy_loop(free_data_func, pCurrentNode->left);
	}
	if (pCurrentNode->right != NULL)
	{
		avl_tree_destroy_loop(free_data_func, pCurrentNode->right);
	}
	
	if (free_data_func != NULL)
	{
		free_data_func(pCurrentNode->data);
	}
	free(pCurrentNode);
}

void avl_tree_destroy(AVLTreeInfo *tree)
{
	if (tree == NULL)
	{
		return;
	}

	if (tree->root != NULL)
	{
		avl_tree_destroy_loop(tree->free_data_func, tree->root);
		tree->root = NULL;
	}
}

static AVLTreeNode *createTreeNode(AVLTreeNode *pParentNode, void *target_data)
{
	AVLTreeNode *pNewNode;
	pNewNode = (AVLTreeNode *)malloc(sizeof(AVLTreeNode));
	if (pNewNode == NULL)
	{
		/*
		fprintf(stderr, "file: "__FILE__", line: %d, " \
			"malloc %d bytes fail!\n", __LINE__, \
			(int)sizeof(AVLTreeNode));
		*/
		return NULL;
	}

	pNewNode->left = pNewNode->right = NULL;
	pNewNode->data = target_data;
	pNewNode->balance = 0;

	return pNewNode;
}

static void avlRotateLeft(AVLTreeNode *pRotateNode, AVLTreeNode **ppRaiseNode)
{
	*ppRaiseNode = pRotateNode->right;
	pRotateNode->right = (*ppRaiseNode)->left;
	(*ppRaiseNode)->left = pRotateNode;
}

static void avlRotateRight(AVLTreeNode *pRotateNode, AVLTreeNode **ppRaiseNode)
{
	*ppRaiseNode = pRotateNode->left;
	pRotateNode->left = (*ppRaiseNode)->right;
	(*ppRaiseNode)->right = pRotateNode;
}

static void avlLeftBalanceWhenInsert(AVLTreeNode **pTreeNode, int *taller)
{
	AVLTreeNode *leftsub;
	AVLTreeNode *rightsub;

	leftsub = (*pTreeNode)->left;
	switch (leftsub->balance)
	{
		case -1 :
			(*pTreeNode)->balance = leftsub->balance = 0;
			avlRotateRight (*pTreeNode, pTreeNode);
			*taller = 0;
			break;
		case 0 :
			break;
		case 1 :
			rightsub = leftsub->right;
			switch ( rightsub->balance )
			{
				case -1:
					(*pTreeNode)->balance = 1;
					leftsub->balance = 0;
					break;
				case 0 :
					(*pTreeNode)->balance = leftsub->balance = 0;
					break;
				case 1 :
					(*pTreeNode)->balance = 0;
					leftsub->balance = -1;
					break;
			}

			rightsub->balance = 0;
			avlRotateLeft( leftsub, &((*pTreeNode)->left));
			avlRotateRight(*pTreeNode, pTreeNode);
			*taller = 0;
	}
}

static void avlRightBalanceWhenInsert(AVLTreeNode **pTreeNode, int *taller)
{
	AVLTreeNode *rightsub;
	AVLTreeNode *leftsub;

	rightsub = (*pTreeNode)->right;
	switch (rightsub->balance)
	{
		case 1: 
			(*pTreeNode)->balance = rightsub->balance = 0;
			avlRotateLeft(*pTreeNode, pTreeNode);
			*taller = 0;
			break;
		case 0: 
			break; 
		case -1:
			leftsub = rightsub->left;
			switch (leftsub->balance)
			{ 
				case 1 :
					(*pTreeNode)->balance = -1;
					rightsub->balance = 0;
					break;
				case 0 :
					(*pTreeNode)->balance = rightsub->balance = 0;
					break;
				case -1 :
					(*pTreeNode)->balance = 0;
					rightsub->balance = 1;
					break;
			}

			leftsub->balance = 0;
			avlRotateRight(rightsub, &((*pTreeNode)->right));
			avlRotateLeft(*pTreeNode, pTreeNode);
			*taller = 0;
	}
}

static int avl_tree_insert_loop(CompareFunc compare_func, AVLTreeNode **pCurrentNode, \
		void *target_data, int *taller)
{
	int nCompRes;
	int success;

	if (*pCurrentNode == NULL)
	{
		*pCurrentNode = createTreeNode(*pCurrentNode, target_data);
		if (*pCurrentNode != NULL)
		{
			*taller = 1;
			return 1;
		}
		else
		{
			return -ENOMEM;
		}
	}

	nCompRes = compare_func((*pCurrentNode)->data, target_data);
	if (nCompRes > 0)
	{
		success = avl_tree_insert_loop(compare_func, \
				&((*pCurrentNode)->left), target_data, taller);
		if (*taller != 0)
		{
			switch ((*pCurrentNode)->balance)
			{
				case -1:
					avlLeftBalanceWhenInsert(pCurrentNode, taller);
					break;
				case 0:
					(*pCurrentNode)->balance = -1;
					break;
				case 1:
					(*pCurrentNode)->balance = 0;
					*taller = 0;
					break;
			}
		}
	}
	else if (nCompRes < 0)
	{
		success = avl_tree_insert_loop(compare_func, \
				&((*pCurrentNode)->right), target_data, taller);
		if (*taller != 0)
		{
			switch ((*pCurrentNode)->balance)
			{
				case -1: 
					(*pCurrentNode)->balance = 0;
					*taller = 0;
					break;
				case 0:
					(*pCurrentNode)->balance = 1; 
					break;
				case 1:
					avlRightBalanceWhenInsert(pCurrentNode, taller);
					break;
			}
		}
	}
	else
	{
		return 0;
	}

	return success;
}

int avl_tree_insert(AVLTreeInfo *tree, void *data)
{
	int taller;

	taller = 0;
	return avl_tree_insert_loop(tree->compare_func, &(tree->root), \
				data, &taller);
}

static int avl_tree_replace_loop(CompareFunc compare_func, \
		FreeDataFunc free_data_func, AVLTreeNode **pCurrentNode, \
		void *target_data, int *taller)
{
	int nCompRes;
	int success;

	if (*pCurrentNode == NULL )
	{
		*pCurrentNode = createTreeNode(*pCurrentNode, target_data);
		if (*pCurrentNode != NULL)
		{
			*taller = 1;
			return 1;
		}
		else
		{
			return -ENOMEM;
		}
	}

	nCompRes = compare_func((*pCurrentNode)->data, target_data);
	if (nCompRes > 0)
	{
		success = avl_tree_replace_loop(compare_func, free_data_func, 
				&((*pCurrentNode)->left), target_data, taller);
		if (*taller != 0)
		{
			switch ((*pCurrentNode)->balance)
			{
				case -1:
					avlLeftBalanceWhenInsert(pCurrentNode, taller);
					break;
				case 0:
					(*pCurrentNode)->balance = -1;
					break;
				case 1:
					(*pCurrentNode)->balance = 0;
					*taller = 0;
					break;
			}
		}
	}
	else if (nCompRes < 0)
	{
		success = avl_tree_replace_loop(compare_func, free_data_func, 
				&((*pCurrentNode)->right), target_data, taller);
		if (*taller != 0) 
		{
			switch ((*pCurrentNode)->balance)
			{
				case -1 : 
					(*pCurrentNode)->balance = 0;
					*taller = 0;
					break;
				case 0 :
					(*pCurrentNode)->balance = 1; 
					break;
				case 1 :
					avlRightBalanceWhenInsert(pCurrentNode, taller);
					break;
			}
		}
	}
	else
	{
		if (free_data_func != NULL)
		{
			free_data_func((*pCurrentNode)->data);
		}
		(*pCurrentNode)->data = target_data;
		return 0;
	}

	return success;
}

int avl_tree_replace(AVLTreeInfo *tree, void *data)
{
	int taller;

	taller = 0;
	return avl_tree_replace_loop(tree->compare_func, \
			tree->free_data_func, &(tree->root), data, &taller);
}

static AVLTreeNode *avl_tree_find_loop(CompareFunc compare_func, \
		AVLTreeNode *pCurrentNode, void *target_data)
{
	int nCompRes;
	nCompRes = compare_func(pCurrentNode->data, target_data);
	if (nCompRes > 0)
	{
		if (pCurrentNode->left == NULL)
		{
			return NULL;
		}
		else
		{
			return avl_tree_find_loop(compare_func, \
				pCurrentNode->left, target_data);
		}
	}
	else if (nCompRes < 0)
	{
		if (pCurrentNode->right == NULL)
		{
			return NULL;
		}
		else
		{
			return avl_tree_find_loop(compare_func, \
				pCurrentNode->right, target_data);
		}
	}
	else
	{
		return pCurrentNode;
	}
}

static void *avl_tree_find_ge_loop(CompareFunc compare_func, \
		AVLTreeNode *pCurrentNode, void *target_data)
{
	int nCompRes;
	void *found;

	nCompRes = compare_func(pCurrentNode->data, target_data);
	if (nCompRes > 0)
	{
		if (pCurrentNode->left == NULL)
		{
			return pCurrentNode->data;
		}

		found = avl_tree_find_ge_loop(compare_func, \
				pCurrentNode->left, target_data);
		return found != NULL ? found : pCurrentNode->data;
	}
	else if (nCompRes < 0)
	{
		if (pCurrentNode->right == NULL)
		{
			return NULL;
		}
		else
		{
			return avl_tree_find_ge_loop(compare_func, \
				pCurrentNode->right, target_data);
		}
	}
	else
	{
		return pCurrentNode->data;
	}
}

void *avl_tree_find(AVLTreeInfo *tree, void *target_data)
{
	AVLTreeNode *found;

	if (tree->root == NULL)
	{
		return NULL;
	}

	found = avl_tree_find_loop(tree->compare_func, \
			tree->root, target_data);

	return found != NULL ? found->data : NULL;
}

void *avl_tree_find_ge(AVLTreeInfo *tree, void *target_data)
{
	void *found;

	if (tree->root == NULL)
	{
		found = NULL;
	}
	else
	{
		found = avl_tree_find_ge_loop(tree->compare_func, \
			tree->root, target_data);
	}

	return found;
}

static void avlLeftBalanceWhenDelete(AVLTreeNode **pTreeNode, int *shorter)
{
	AVLTreeNode *leftsub;
	AVLTreeNode *rightsub;

	leftsub = (*pTreeNode)->left;
	switch (leftsub->balance)
	{
		case -1:
			(*pTreeNode)->balance = leftsub->balance = 0;
			avlRotateRight (*pTreeNode, pTreeNode);
			break;
		case 0:
			leftsub->balance = 1;
			avlRotateRight(*pTreeNode, pTreeNode);
			*shorter = 0;
			break;
		case 1:
			rightsub = leftsub->right;
			switch ( rightsub->balance )
			{
				case -1:
					(*pTreeNode)->balance = 1;
					leftsub->balance = 0;
					break;
				case 0 :
					(*pTreeNode)->balance = leftsub->balance = 0;
					break;
				case 1 :
					(*pTreeNode)->balance = 0;
					leftsub->balance = -1;
					break;
			}

			rightsub->balance = 0;
			avlRotateLeft( leftsub, &((*pTreeNode)->left));
			avlRotateRight(*pTreeNode, pTreeNode);
			break;
	}
}

static void avlRightBalanceWhenDelete(AVLTreeNode **pTreeNode, int *shorter)
{
	AVLTreeNode *rightsub;
	AVLTreeNode *leftsub;

	rightsub = (*pTreeNode)->right;
	switch (rightsub->balance)
	{
		case 1:
			(*pTreeNode)->balance = rightsub->balance = 0;
			avlRotateLeft(*pTreeNode, pTreeNode);
			break;
		case 0:
			rightsub->balance = -1;
			avlRotateLeft(*pTreeNode, pTreeNode);
			*shorter = 0;
			break;
		case -1:
			leftsub = rightsub->left;
			switch (leftsub->balance)
			{
				case 1:
					(*pTreeNode)->balance = -1;
					rightsub->balance = 0;
					break;
				case 0:
					(*pTreeNode)->balance = rightsub->balance = 0;
					break;
				case -1:
					(*pTreeNode)->balance = 0;
					rightsub->balance = 1;
					break;
			}

			leftsub->balance = 0;
			avlRotateRight(rightsub, &((*pTreeNode)->right));
			avlRotateLeft(*pTreeNode, pTreeNode);
			break;
	}
}

static int avl_tree_delete_loop(AVLTreeInfo *tree, AVLTreeNode **pCurrentNode,\
		 void *target_data, int *shorter, AVLTreeNode *pDeletedDataNode)
{
	int nCompRes;
	bool result;
	AVLTreeNode *leftsub;
	AVLTreeNode *rightsub;

	if (pDeletedDataNode != NULL)
	{
		if ((*pCurrentNode)->right == NULL)
		{
			pDeletedDataNode->data = (*pCurrentNode)->data;
			leftsub = (*pCurrentNode)->left;

			free(*pCurrentNode);
			*pCurrentNode = leftsub;
			*shorter = 1;
			return 1;
		}

		nCompRes = -1;
	}
	else
	{
		nCompRes = tree->compare_func((*pCurrentNode)->data, target_data);
	}

	if (nCompRes > 0)
	{
		if ((*pCurrentNode)->left == NULL)
		{
			return 0;
		}

		result = avl_tree_delete_loop(tree, &((*pCurrentNode)->left), \
				target_data, shorter, pDeletedDataNode);
		if (*shorter != 0)
		{
			switch ((*pCurrentNode)->balance)
			{
			  case -1:
				(*pCurrentNode)->balance = 0;
				break;
			  case 0:
				(*pCurrentNode)->balance = 1;
				*shorter = 0;
				break;
			  case 1:
				avlRightBalanceWhenDelete(pCurrentNode, shorter);
				break;
			}
		}
		return result;
	}
	else if (nCompRes < 0)
	{
		if ((*pCurrentNode)->right == NULL)
		{
			return 0;
		}

		result = avl_tree_delete_loop(tree, &((*pCurrentNode)->right), \
				target_data, shorter, pDeletedDataNode);
		if (*shorter != 0)
		{
			switch ((*pCurrentNode)->balance)
			{
				case -1:
		          		avlLeftBalanceWhenDelete(pCurrentNode, shorter);
		          		break;
		        	case 0:
		          		(*pCurrentNode)->balance = -1;
				  	*shorter = 0;
		          		break;
		        	case 1:
		  	        	(*pCurrentNode)->balance = 0;
		        	  	break;
		    	}
		}
		return result;
	}
	else
	{
		if (tree->free_data_func != NULL)
		{
			tree->free_data_func((*pCurrentNode)->data);
		}

		leftsub = (*pCurrentNode)->left;
		rightsub = (*pCurrentNode)->right;
		if (leftsub == NULL)
		{
			free(*pCurrentNode);
			*pCurrentNode = rightsub;
		}
		else if (rightsub == NULL)
		{
			free(*pCurrentNode);
			*pCurrentNode = leftsub;
		}
		else
		{
			avl_tree_delete_loop(tree, &((*pCurrentNode)->left), \
				target_data, shorter, *pCurrentNode);

			if (*shorter != 0)
			{
				switch ((*pCurrentNode)->balance)
				{
				  case -1:
					(*pCurrentNode)->balance = 0;
					break;
				  case 0:
					(*pCurrentNode)->balance = 1;
					*shorter = 0;
					break;
				  case 1:
					avlRightBalanceWhenDelete(pCurrentNode, shorter);
					break;
				}
			}
			return 1;
		}

		*shorter = 1;
		return 1;
	}
}

int avl_tree_delete(AVLTreeInfo *tree, void *data)
{
	int shorter;

	if (tree->root == NULL)
	{
		return 0;
	}

	shorter = 0;
	return avl_tree_delete_loop(tree, &(tree->root), data, &shorter, NULL);
}

static int avl_tree_walk_loop(DataOpFunc data_op_func, \
		AVLTreeNode *pCurrentNode, void *args)
{
	int result;

	if (pCurrentNode->left != NULL)
	{
		result = avl_tree_walk_loop(data_op_func, \
				pCurrentNode->left, args);
		if (result != 0)
		{
			return result;
		}
	}

	if ((result=data_op_func(pCurrentNode->data, args)) != 0)
	{
		return result;
	}

	/*
	if (pCurrentNode->balance >= -1 && pCurrentNode->balance <= 1)
	{
		//printf("==%d\n", pCurrentNode->balance);
	}
	else
	{
		printf("==bad %d!!!!!!!!!!!!\n", pCurrentNode->balance);
	}
	*/

	if (pCurrentNode->right != NULL)
	{
		result = avl_tree_walk_loop(data_op_func, \
				pCurrentNode->right, args);
	}

	return result;
}

int avl_tree_walk(AVLTreeInfo *tree, DataOpFunc data_op_func, void *args)
{
	if (tree->root == NULL)
	{
		return 0;
	}
	else
	{
		return avl_tree_walk_loop(data_op_func, tree->root, args);
	}
}

static void avl_tree_count_loop(AVLTreeNode *pCurrentNode, int *count)
{
	if (pCurrentNode->left != NULL)
	{
		avl_tree_count_loop(pCurrentNode->left, count);
	}

	(*count)++;

	if (pCurrentNode->right != NULL)
	{
		avl_tree_count_loop(pCurrentNode->right, count);
	}
}

int avl_tree_count(AVLTreeInfo *tree)
{
	int count;
	if (tree->root == NULL)
	{
		return 0;
	}

	count = 0;
	avl_tree_count_loop(tree->root, &count);
	return count;
}

int avl_tree_depth(AVLTreeInfo *tree)
{
	int depth;
	AVLTreeNode *pNode;

	if (tree->root == NULL)
	{
		return 0;
	}

	depth = 0;
	pNode = tree->root;
	while (pNode != NULL)
	{
		if (pNode->balance == -1)
		{
			pNode = pNode->left;
		}
		else
		{
			pNode = pNode->right;
		}
		depth++;
	}

	return depth;
}

/*
static void avl_tree_print_loop(AVLTreeNode *pCurrentNode)
{
	printf("%ld  left: %ld, right: %ld, balance: %d\n", pCurrentNode->data, 
		pCurrentNode->left != NULL ? pCurrentNode->left->data : 0,
		pCurrentNode->right != NULL ? pCurrentNode->right->data : 0,
		pCurrentNode->balance);

	if (pCurrentNode->left != NULL)
	{
		avl_tree_print_loop(pCurrentNode->left);
	}

	if (pCurrentNode->right != NULL)
	{
		avl_tree_print_loop(pCurrentNode->right);
	}
}

void avl_tree_print(AVLTreeInfo *tree)
{
	if (tree->root == NULL)
	{
		return;
	}

	avl_tree_print_loop(tree->root);
}
*/

