/*
 * rbtree.c -- generic red black tree
 *
 * Lifted from Unbound and ldns
 *
 * Copyright (c) 2001-2008, NLnet Labs. All rights reserved.
 *
 * This software is open source.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of the NLNET LABS nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

/**
 * \file
 * Implementation of a redblack tree.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "rbtree.h"


/** Node colour black */
#define BLACK   0
/** Node colour red */
#define RED     1

/** the NULL node, global alloc */
ramfs_rbnode_t ramfs_rbtree_null_node = {
    RAMFS_RBTREE_NULL,    /* Parent.  */
    RAMFS_RBTREE_NULL,    /* Left.  */
    RAMFS_RBTREE_NULL,    /* Right.  */
    NULL,                 /* Key.  */
    BLACK                 /* Color.  */
};

/** rotate subtree left (to preserve redblack property) */
static void ramfs_rbtree_rotate_left(ramfs_rbtree_t *rbtree,
                                     ramfs_rbnode_t *node);
/** rotate subtree right (to preserve redblack property) */
static void ramfs_rbtree_rotate_right(ramfs_rbtree_t *rbtree,
                                      ramfs_rbnode_t *node);
/** Fixup node colours when insert happened */
static void ramfs_rbtree_insert_fixup(ramfs_rbtree_t *rbtree,
                                      ramfs_rbnode_t *node);
/** Fixup node colours when delete happened */
static void ramfs_rbtree_delete_fixup(ramfs_rbtree_t *rbtree,
                                      ramfs_rbnode_t *child,
                                      ramfs_rbnode_t *child_parent);

/*
 * Creates a new red black tree, intializes and returns a pointer to it.
 *
 * Return NULL on failure.
 */
ramfs_rbtree_t *ramfs_rbtree_create(int (*cmpf)(const void *, const void *))
{
    ramfs_rbtree_t *rbtree;

    /* Allocate memory for it */
    rbtree = (ramfs_rbtree_t *) malloc(sizeof(ramfs_rbtree_t));
    if (!rbtree) {
        return NULL;
    }

    /* Initialize it */
    ramfs_rbtree_init(rbtree, cmpf);

    return rbtree;
}

void ramfs_rbtree_init(ramfs_rbtree_t *rbtree,
                       int (*cmpf)(const void *, const void *))
{
    /* Initialize it */
    rbtree->root = RAMFS_RBTREE_NULL;
    rbtree->count = 0;
    rbtree->cmp = cmpf;
}

void ramfs_rbtree_free(ramfs_rbtree_t *rbtree)
{
    free(rbtree);
}

/*
 * Rotates the node to the left.
 */
static void ramfs_rbtree_rotate_left(ramfs_rbtree_t *rbtree,
                                     ramfs_rbnode_t *node)
{
    ramfs_rbnode_t *right = node->right;
    node->right = right->left;
    if (right->left != RAMFS_RBTREE_NULL) {
        right->left->parent = node;
    }

    right->parent = node->parent;

    if (node->parent != RAMFS_RBTREE_NULL) {
        if (node == node->parent->left) {
            node->parent->left = right;
        } else  {
            node->parent->right = right;
        }
    } else {
        rbtree->root = right;
    }
    right->left = node;
    node->parent = right;
}

/*
 * Rotates the node to the right.
 */
static void ramfs_rbtree_rotate_right(ramfs_rbtree_t *rbtree,
                                      ramfs_rbnode_t *node)
{
    ramfs_rbnode_t *left = node->left;
    node->left = left->right;
    if (left->right != RAMFS_RBTREE_NULL) {
        left->right->parent = node;
    }

    left->parent = node->parent;

    if (node->parent != RAMFS_RBTREE_NULL) {
        if (node == node->parent->right) {
            node->parent->right = left;
        } else  {
            node->parent->left = left;
        }
    } else {
        rbtree->root = left;
    }
    left->right = node;
    node->parent = left;
}

static void ramfs_rbtree_insert_fixup(ramfs_rbtree_t *rbtree,
                                      ramfs_rbnode_t *node)
{
    ramfs_rbnode_t *uncle;

    /* While not at the root and need fixing... */
    while (node != rbtree->root && node->parent->color == RED) {
        /* If our parent is left child of our grandparent... */
        if (node->parent == node->parent->parent->left) {
            uncle = node->parent->parent->right;

            /* If our uncle is red... */
            if (uncle->color == RED) {
                /* Paint the parent and the uncle black... */
                node->parent->color = BLACK;
                uncle->color = BLACK;

                /* And the grandparent red... */
                node->parent->parent->color = RED;

                /* And continue fixing the grandparent */
                node = node->parent->parent;
            } else { /* Our uncle is black... */
                /* Are we the right child? */
                if (node == node->parent->right) {
                    node = node->parent;
                    ramfs_rbtree_rotate_left(rbtree, node);
                }
                /* Now we're the left child, repaint and rotate... */
                node->parent->color = BLACK;
                node->parent->parent->color = RED;
                ramfs_rbtree_rotate_right(rbtree, node->parent->parent);
            }
        } else {
            uncle = node->parent->parent->left;

            /* If our uncle is red... */
            if (uncle->color == RED) {
                /* Paint the parent and the uncle black... */
                node->parent->color = BLACK;
                uncle->color = BLACK;

                /* And the grandparent red... */
                node->parent->parent->color = RED;

                /* And continue fixing the grandparent */
                node = node->parent->parent;
            } else { /* Our uncle is black... */
                /* Are we the right child? */
                if (node == node->parent->left) {
                    node = node->parent;
                    ramfs_rbtree_rotate_right(rbtree, node);
                }
                /* Now we're the right child, repaint and rotate... */
                node->parent->color = BLACK;
                node->parent->parent->color = RED;
                ramfs_rbtree_rotate_left(rbtree, node->parent->parent);
            }
        }
    }
    rbtree->root->color = BLACK;
}

/*
 * Inserts a node into a red black tree.
 *
 * Returns NULL on failure or the pointer to the newly added node
 * otherwise.
 */
ramfs_rbnode_t *ramfs_rbtree_insert(ramfs_rbtree_t *rbtree,
                                    ramfs_rbnode_t *data)
{
    /* XXX Not necessary, but keeps compiler quiet... */
    int r = 0;

    /* We start at the root of the tree */
    ramfs_rbnode_t *node = rbtree->root;
    ramfs_rbnode_t *parent = RAMFS_RBTREE_NULL;

    /* Lets find the new parent... */
    while (node != RAMFS_RBTREE_NULL) {
        /* Compare two keys, do we have a duplicate? */
        if ((r = rbtree->cmp(data->key, node->key)) == 0) {
            return NULL;
        }
        parent = node;

        if (r < 0) {
            node = node->left;
        } else {
            node = node->right;
        }
    }

    /* Initialize the new node */
    data->parent = parent;
    data->left = data->right = RAMFS_RBTREE_NULL;
    data->color = RED;
    rbtree->count++;

    /* Insert it into the tree... */
    if (parent != RAMFS_RBTREE_NULL) {
        if (r < 0) {
            parent->left = data;
        } else {
            parent->right = data;
        }
    } else {
        rbtree->root = data;
    }

    /* Fix up the red-black properties... */
    ramfs_rbtree_insert_fixup(rbtree, data);

    return data;
}

/*
 * Searches the red black tree, returns the data if key is found or NULL
 * otherwise.
 */
ramfs_rbnode_t *ramfs_rbtree_search(ramfs_rbtree_t *rbtree, const void *key)
{
    ramfs_rbnode_t *node;

    if (ramfs_rbtree_find_less_equal(rbtree, key, &node)) {
        return node;
    } else {
        return NULL;
    }
}

/** helpers for delete: swap node colours */
static void swap_int8(uint8_t *x, uint8_t *y)
{
    uint8_t t = *x; *x = *y; *y = t;
}

/** helpers for delete: swap node pointers */
static void swap_np(ramfs_rbnode_t **x, ramfs_rbnode_t **y)
{
    ramfs_rbnode_t *t = *x; *x = *y; *y = t;
}

/** Update parent pointers of child trees of 'parent' */
static void change_parent_ptr(ramfs_rbtree_t *rbtree, ramfs_rbnode_t *parent,
                              ramfs_rbnode_t *old, ramfs_rbnode_t *new)
{
    if (parent == RAMFS_RBTREE_NULL) {
        if (rbtree->root == old) {
            rbtree->root = new;
        }
        return;
    }
    if (parent->left == old) {
        parent->left = new;
    }
    if (parent->right == old) {
        parent->right = new;
    }
}
/** Update parent pointer of a node 'child' */
static void change_child_ptr(ramfs_rbnode_t *child, ramfs_rbnode_t *old,
                             ramfs_rbnode_t *new)
{
    if (child == RAMFS_RBTREE_NULL) {
        return;
    }
    if (child->parent == old) {
        child->parent = new;
    }
}

ramfs_rbnode_t *ramfs_rbtree_delete_node(ramfs_rbtree_t *rbtree,
                                         ramfs_rbnode_t *node)
{
    ramfs_rbnode_t *child;
    rbtree->count--;

    /* make sure we have at most one non-leaf child */
    if (node->left != RAMFS_RBTREE_NULL && node->right != RAMFS_RBTREE_NULL) {
        /* swap with smallest from right subtree (or largest from left) */
        ramfs_rbnode_t *smright = node->right;
        while (smright->left != RAMFS_RBTREE_NULL) {
            smright = smright->left;
        }
        /* swap the smright and to_delete elements in the tree,
         * but the ramfs_rbnode_t is first part of user data struct
         * so cannot just swap the keys and data pointers. Instead
         * readjust the pointers left,right,parent */

        /* swap colors - colors are tied to the position in the tree */
        swap_int8(&node->color, &smright->color);

        /* swap child pointers in parents of smright/to_delete */
        change_parent_ptr(rbtree, node->parent, node, smright);
        if (node->right != smright) {
            change_parent_ptr(rbtree, smright->parent, smright, node);
        }

        /* swap parent pointers in children of smright/to_delete */
        change_child_ptr(smright->left, smright, node);
        change_child_ptr(smright->left, smright, node);
        change_child_ptr(smright->right, smright, node);
        change_child_ptr(smright->right, smright, node);
        change_child_ptr(node->left, node, smright);
        if (node->right != smright) {
            change_child_ptr(node->right, node, smright);
        }
        if (node->right == smright) {
            /* set up so after swap they work */
            node->right = node;
            smright->parent = smright;
        }

        /* swap pointers in to_delete/smright nodes */
        swap_np(&node->parent, &smright->parent);
        swap_np(&node->left, &smright->left);
        swap_np(&node->right, &smright->right);

        /* now delete to_delete (which is at the location where the smright
         * previously was) */
    }

    if (node->left != RAMFS_RBTREE_NULL) {
        child = node->left;
    } else {
        child = node->right;
    }

    /* unlink to_delete from the tree, replace to_delete with child */
    change_parent_ptr(rbtree, node->parent, node, child);
    change_child_ptr(child, node, node->parent);

    if (node->color == RED) {
        /* if node is red then the child (black) can be swapped in */
    } else if (child->color == RED) {
        /* change child to BLACK, removing a RED node is no problem */
        if (child!=RAMFS_RBTREE_NULL) {
            child->color = BLACK;
        }
    } else {
        ramfs_rbtree_delete_fixup(rbtree, child, node->parent);
    }

    /* unlink completely */
    node->parent = RAMFS_RBTREE_NULL;
    node->left = RAMFS_RBTREE_NULL;
    node->right = RAMFS_RBTREE_NULL;
    node->color = BLACK;
    return node;
}

ramfs_rbnode_t *ramfs_rbtree_delete(ramfs_rbtree_t *rbtree, const void *key)
{
    ramfs_rbnode_t *to_delete;
    if ((to_delete = ramfs_rbtree_search(rbtree, key)) == 0) {
        return 0;
    }
    return ramfs_rbtree_delete_node(rbtree, to_delete);
}

static void ramfs_rbtree_delete_fixup(ramfs_rbtree_t *rbtree,
                                      ramfs_rbnode_t *child,
                                      ramfs_rbnode_t *child_parent)
{
    ramfs_rbnode_t* sibling;
    int go_up = 1;

    /* determine sibling to the node that is one-black short */
    if (child_parent->right == child) {
        sibling = child_parent->left;
    } else {
        sibling = child_parent->right;
    }

    while (go_up) {
        if (child_parent == RAMFS_RBTREE_NULL) {
            /* removed parent==black from root, every path, so ok */
            return;
        }

        if (sibling->color == RED) { /* rotate to get a black sibling */
            child_parent->color = RED;
            sibling->color = BLACK;
            if (child_parent->right == child) {
                ramfs_rbtree_rotate_right(rbtree, child_parent);
            } else {
                ramfs_rbtree_rotate_left(rbtree, child_parent);
            }
            /* new sibling after rotation */
            if (child_parent->right == child) {
                sibling = child_parent->left;
            } else {
                sibling = child_parent->right;
            }
        }

        if (child_parent->color == BLACK
                && sibling->color == BLACK
                && sibling->left->color == BLACK
                && sibling->right->color == BLACK) {
            /* fixup local with recolor of sibling */
            if (sibling != RAMFS_RBTREE_NULL) {
                sibling->color = RED;
            }

            child = child_parent;
            child_parent = child_parent->parent;
            /* prepare to go up, new sibling */
            if (child_parent->right == child) {
                sibling = child_parent->left;
            } else {
                sibling = child_parent->right;
            }
        } else {
            go_up = 0;
        }
    }

    if (child_parent->color == RED
            && sibling->color == BLACK
            && sibling->left->color == BLACK
            && sibling->right->color == BLACK) {
        /* move red to sibling to rebalance */
        if (sibling != RAMFS_RBTREE_NULL) {
            sibling->color = RED;
        }
        child_parent->color = BLACK;
        return;
    }

    /* get a new sibling, by rotating at sibling. See which child of sibling
     * is red */
    if (child_parent->right == child
            && sibling->color == BLACK
            && sibling->right->color == RED
            && sibling->left->color == BLACK) {
        sibling->color = RED;
        sibling->right->color = BLACK;
        ramfs_rbtree_rotate_left(rbtree, sibling);
        /* new sibling after rotation */
        if (child_parent->right == child) {
            sibling = child_parent->left;
        } else {
            sibling = child_parent->right;
        }
    } else if(child_parent->left == child
            && sibling->color == BLACK
            && sibling->left->color == RED
            && sibling->right->color == BLACK) {
        sibling->color = RED;
        sibling->left->color = BLACK;
        ramfs_rbtree_rotate_right(rbtree, sibling);
        /* new sibling after rotation */
        if (child_parent->right == child) {
            sibling = child_parent->left;
        } else {
            sibling = child_parent->right;
        }
    }

    /* now we have a black sibling with a red child. rotate and exchange
     * colors. */
    sibling->color = child_parent->color;
    child_parent->color = BLACK;
    if (child_parent->right == child) {
        sibling->left->color = BLACK;
        ramfs_rbtree_rotate_right(rbtree, child_parent);
    } else {
        sibling->right->color = BLACK;
        ramfs_rbtree_rotate_left(rbtree, child_parent);
    }
}

int ramfs_rbtree_find_less_equal(ramfs_rbtree_t *rbtree, const void *key,
                                 ramfs_rbnode_t **result)
{
    int r;
    ramfs_rbnode_t *node;

    /* We start at root... */
    node = rbtree->root;

    *result = NULL;

    /* While there are children... */
    while (node != RAMFS_RBTREE_NULL) {
        r = rbtree->cmp(key, node->key);
        if (r == 0) {
            /* Exact match */
            *result = node;
            return 1;
        }
        if (r < 0) {
            node = node->left;
        } else {
            /* Temporary match */
            *result = node;
            node = node->right;
        }
    }
    return 0;
}

/*
 * Finds the first element in the red black tree
 */
ramfs_rbnode_t *ramfs_rbtree_first(ramfs_rbtree_t *rbtree)
{
    ramfs_rbnode_t *node = NULL;

    if (rbtree->root != RAMFS_RBTREE_NULL) {
        for (node = rbtree->root;
                node->left != RAMFS_RBTREE_NULL;
                node = node->left)
            ;
    }
    return node;
}

ramfs_rbnode_t *ramfs_rbtree_last(ramfs_rbtree_t *rbtree)
{
    ramfs_rbnode_t *node = NULL;

    if (rbtree->root != RAMFS_RBTREE_NULL) {
        for (node = rbtree->root;
                node->right != RAMFS_RBTREE_NULL;
                node = node->right)
            ;
    }
    return node;
}

/*
 * Returns the next node...
 */
ramfs_rbnode_t *ramfs_rbtree_next(ramfs_rbnode_t *node)
{
    ramfs_rbnode_t *parent;

    if (node->right != RAMFS_RBTREE_NULL) {
        /* One right, then keep on going left... */
        for (node = node->right;
                node->left != RAMFS_RBTREE_NULL;
                node = node->left)
            ;
    } else {
        parent = node->parent;
        while (parent != RAMFS_RBTREE_NULL && node == parent->right) {
            node = parent;
            parent = parent->parent;
        }
        node = parent;
    }
    return node;
}

ramfs_rbnode_t *ramfs_rbtree_previous(ramfs_rbnode_t *node)
{
    ramfs_rbnode_t *parent;

    if (node->left != RAMFS_RBTREE_NULL) {
        /* One left, then keep on going right... */
        for (node = node->left;
                node->right != RAMFS_RBTREE_NULL;
                node = node->right)
            ;
    } else {
        parent = node->parent;
        while (parent != RAMFS_RBTREE_NULL && node == parent->left) {
            node = parent;
            parent = parent->parent;
        }
        node = parent;
    }
    return node;
}
