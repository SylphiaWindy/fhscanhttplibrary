#include "Build.h"
#include "Tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/******************************************************************************/
/******************************************************************************/
TreeNode::TreeNode()
{
	TreeNode(NULL,NULL);
}
/******************************************************************************/
/******************************************************************************/
TreeNode::TreeNode(const char *lpTreeNodeName)
{
	TreeNode(lpTreeNodeName,NULL);
}
/******************************************************************************/
/******************************************************************************/

TreeNode::TreeNode(const char *lpTreeNodeName,TreeNode *Parent)
{
	text = NULL;
	count = 0;
	left = NULL;
	right = NULL;
	ParentItem = NULL;

	ParentTree = NULL;
	ChildTree   = NULL;
	data	   = NULL;
	if (lpTreeNodeName) SetTreeNodeName(lpTreeNodeName);
	if (Parent) SetTreeNodeParentItem(Parent);
}
/******************************************************************************/
/******************************************************************************/
TreeNode::~TreeNode()
{
	count = 0;
	if (left)
	{
		delete (left);
		left = NULL;
	}
	if (right)
	{
		delete (right);
		right = NULL;
	}
	if (text) 
	{
		free(text);
		text = NULL;
	}
	ParentItem = NULL;
	ParentTree = NULL;

	if (ChildTree) 
	{
		delete ChildTree;
		ChildTree   = NULL;
	}
	data	   = NULL;
}
/******************************************************************************/
/******************************************************************************/
void TreeNode::SetTreeNodeName(const char *lpTreeNodeName)
{
	if (text) free(text);
	if (lpTreeNodeName)
	{
		text = strdup(lpTreeNodeName);
	} else {
		text = NULL;
	}
}

/******************************************************************************/
/******************************************************************************/
void TreeNode::SetTreeNodeRight(TreeNode *newright) 
{
	right = newright; 


	TreeNode *parent = this;

	do
	{
		parent->count++;
		parent = parent->ParentItem;

	} while (parent);

}
/******************************************************************************/
void TreeNode::SetTreeNodeLeft(TreeNode *newleft) { 

	left = newleft; 

	TreeNode *parent = this;
	do
	{
		parent->count++;
		parent = parent->ParentItem;

	} while (parent);
}
/******************************************************************************/
class TreeNode*	TreeNode::GetTreeNodeItemID(int n) { 
	return (ParentTree->GetTreeNodeItemID(n)) ; 
}
/******************************************************************************/
/******************************************************************************/
class bTree* TreeNode::GetNewTreeNodeSubTree()
{
	ChildTree = new bTree;
	return(ChildTree);
}
/******************************************************************************/
/******************************************************************************/
class bTree* TreeNode::GetNewTreeNodeSubTree(char *lpSubTree)
{
	ChildTree = new bTree(lpSubTree);
	return(ChildTree);
}


class TreeNode* TreeNode::GetTreeNodeParentItemTop(void) 
{ 
	TreeNode* top = this->GetTreeNodeParentItem();
	while (top->GetTreeNodeParentItem())
	{
		top = top->GetTreeNodeParentItem();
	}
	return(top);
}

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
bTree::bTree()
{
	text= NULL;
	root = NULL;
	count = 0;	
}
/******************************************************************************/
bTree::bTree(char *lpTreeName)
{
	if (lpTreeName)
	{
		text= strdup(lpTreeName);
	} else {
		text = NULL;
	}
	root = NULL;
	count = 0;
}
/******************************************************************************/
void bTree::SetTreeName(const char *lpTreeName)
{
	if (text)
	{
		free(text);
		text = NULL;
	}
	if (lpTreeName)
	{
		text = strdup(lpTreeName);
	}
}
/******************************************************************************/
bTree::~bTree()
{
	if (text)
	{
		free(text);
		text = NULL;
	}
	if (root) {
		delete root;
		root = NULL;
	}
	count = 0;
}


/******************************************************************************/


TreeNode *bTree::TreeExistItem( const char *lpTreeItemName)
{
	TreeNode *x;
	if (root==NULL)
	{
		return(NULL);
	}
	/* search the tree */
	x=root;
	while (x != NULL) 
	{
		int ret = strcmp(x->GetTreeNodeName(),lpTreeItemName);

		if (ret==0)
		{
			return(x);
		} else {
			if (ret<0) {
				x = x->GetTreeNodeLeft();
			} else {
				x = x->GetTreeNodeRight();
			}
		}
	}

	return(NULL);
}

/******************************************************************************/
/*
Returns a TreeNode element from the tree.
Elements are returned from the tree as if they come from a linear and sorted array.
*/

TreeNode *bTree::GetTreeNodeItemID(int value)
{
	int n = value +1; /* Cheat to allow the counter to be 0 */
	if ( (count<n) || (n<=0) ) {
		return(NULL);
	}
	TreeNode *node = root;
	int total = 0;
#define NLEFT node->GetTreeNodeLeft()->GetTreeNodeCount() +1
#define NRIGHT node->GetTreeNodeRight()->GetTreeNodeCount() +1
	while ((node!=NULL) && (total<n) ) 
	{
		if (node->GetTreeNodeLeft()) 
		{
			if ( NLEFT + total >= n)
			{
				node = node->GetTreeNodeLeft();
			} else 
			{
				total += NLEFT; //add nodes from left side
				total ++; //Add current node
				if (total == n)
				{
					return (node);
				} else {
					node = node->GetTreeNodeRight();
				}

			}
		} else if (node->GetTreeNodeRight())
		{
			total++;
			if (total == n)
			{
				return (node);
			} else {
				node = node->GetTreeNodeRight();
			}
		} else {
			total++;
			if (total == n)
			{
				return (node);
			} else {
				/* FATAL - ##CRITICAL UNEXPECTED ERROR##: BAD IMPLEMENTATION? */
				return(NULL);
			}
		}
	}
	return(NULL);
}
/******************************************************************************/


/******************************************************************************/

/* Insert a text value into the tree - if the value doesn�t already exists, increment
the count for parent nodes. */

TreeNode* bTree::TreeInsert(const char *str)
{
	return (TreeInsert(str,NULL));
}
 /******************************************************************************/
TreeNode* bTree::TreeInsert(const char *str,TreeNode *ParentItem)
{
	if (ParentItem == NULL)
	{
		if (root==NULL) 
		{
			TreeNode *newnode = new TreeNode(str,NULL);
			newnode->SetTreeNodeParentTree(this);
			root = newnode;
			count++;
			return ( root);
		} else 
		{
			TreeNode *y=NULL;
			TreeNode *x=root;

			while (x != NULL) {
				if (strcmp(x->GetTreeNodeName(), str)==0) 
				{
					/* already Exists */
					return (x);
				}
				y=x;
				if (strcmp(str,x->GetTreeNodeName())<0)
					x = x->GetTreeNodeLeft();
				else
					x = x->GetTreeNodeRight();
			}
			/* str doesn't yet exist in tree - add it in */

			TreeNode *newnode=new TreeNode (str,y);
			newnode->SetTreeNodeParentTree(this);
			if (strcmp(str, y->GetTreeNodeName())<0)
			{
				y->SetTreeNodeLeft(newnode);
			}
			else
			{
				y->SetTreeNodeRight(newnode);
			}		
			this->count++; /* Add an additional element to the tree counter */
			return (newnode);
		}
	} else {
		TreeNode*newnode = ParentItem->GetTreeNodeChildTree()->TreeInsert(str,NULL);
		return(newnode);
	}
}

/******************************************************************************/
/* Print the entire tree in sorted order */

void bTree::SubTreePrint(TreeNode *subtree) {
	if (subtree!=NULL) {
		SubTreePrint(subtree->GetTreeNodeLeft());
		printf("%s: %d\n", subtree->GetTreeNodeName(), subtree->GetTreeNodeCount());
		SubTreePrint(subtree->GetTreeNodeRight());
	}
}

void bTree::TreePrint() {
	SubTreePrint(root);
}



#if 0
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
//Insert a Full path of Items and creates all new nodes
TreeNode *TreeInsertItems(Tree *tree, TreeNode *parent, char *strpath)
{

	TreeNode *node;
	char *str = strdup(strpath);
	int IsFolder =  (str[strlen(str)-1] == '/');
	char *path = strtok(str,"/");

	if (!path) {
		node = TreeInsert(tree,parent,"");
	} else {
		TreeNode *currentparent = parent;
		Tree *base = tree;
		do
		{
			node = TreeInsert(base,currentparent,path);
			path = strtok(NULL,"/");
			if ( (path) || (IsFolder) )
			{
				if (!node->SubItems)
				{
					node->SubItems = TreeInitialize("/");
				}
				currentparent = node;
				base = node->SubItems;
			} else {
				break;
			}
		}  while (path!=NULL);
	}
	free(str);
	return (node);
}




int SubTreeToArray(char **array, TreeNode *subtree, int pos, int n) {
	char line [ MXLINELEN ] ;
	int added=0;

	if (subtree!=NULL) {
		added = SubTreeToArray(array, subtree->left, pos, n);
		if (subtree->count >= n) {
			sprintf(line, "%s (%d)", subtree->text, subtree->count);
			array[pos+added]=strdup(line);
			added++;
		}
		added += SubTreeToArray(array, subtree->right, pos+added, n);
	}
	return added;
}



/* Create a linear array of pointers to char which point to each of the
text items in the tree which have a count of 'n' or greater.
It is the responsibility of the calling application to free the allocated
memory. */

char** TreeToArray(Tree *tree, int n) {
	char **array;
	int size;

	size = TreeCount(tree->root, n);
	array = (char**)malloc (sizeof(char*) * (size+1));
	SubTreeToArray(array, tree->root, 0, n);
	array[size] = NULL;
	return array;
}
#endif
