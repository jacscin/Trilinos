/*====================================================================
 * ------------------------
 * | CVS File Information |
 * ------------------------
 *
 * $RCSfile$
 *
 * $Author$
 *
 * $Date$
 *
 * $Revision$
 *
 * $Name$
 *====================================================================*/
#ifndef lint
static char *cvs_loadbal = "$Id$";
#endif

/*--------------------------------------------------------------------------*/
/* Purpose: Call Zoltan to determine a new load balance.                    */
/*          Contains all of the callback functions that Zoltan needs        */
/*          for the load balancing.                                         */
/*--------------------------------------------------------------------------*/
/* Author(s):  Matthew M. St.John (9226)                                    */
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
/* Revision History:                                                        */
/*    10 May 1999:       Date of creation.                                  */
/*--------------------------------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>

#include <mpi.h>

#include "dr_const.h"
#include "dr_err_const.h"

extern int migrate_elements(int, ELEM_INFO **, struct LB_Struct *, int,
                            LB_GID *, LB_LID *, int *, int, LB_GID *,
                            LB_LID *, int *);

/*
 *  PROTOTYPES for load-balancer interface functions.
 */

LB_NUM_OBJ_FN get_num_elements;
/* not used right now --->
LB_OBJ_LIST_FN get_elements;
*/
LB_FIRST_OBJ_FN get_first_element;
LB_NEXT_OBJ_FN get_next_element;

LB_OBJ_WEIGHT_FN get_elem_weight;

LB_NUM_GEOM_FN get_num_geom;
LB_GEOM_FN get_geom;

int run_zoltan(int Proc, PROB_INFO_PTR prob, ELEM_INFO *elements[])
{
/* Local declarations. */
  struct LB_Struct *lb_obj;

  /* Variables returned by the load balancer */
  LB_GID *import_gids = NULL;    /* Global node nums of nodes to be imported */
  LB_LID *import_lids = NULL;    /* Pointers to nodes to be imported         */
  int   *import_procs = NULL;    /* Proc IDs of procs owning nodes to be
                                    imported.                                */
  LB_GID *export_gids = NULL;    /* Global node nums of nodes to be exported */
  LB_LID *export_lids = NULL;    /* Pointers to nodes to be exported         */
  int   *export_procs = NULL;    /* Proc IDs of destination procs for nodes
                                    to be exported.                          */
  int num_imported;              /* Number of nodes to be imported.          */
  int num_exported;              /* Number of nodes to be exported.          */
  int new_decomp;                /* Flag indicating whether the decomposition
                                    has changed                              */

/***************************** BEGIN EXECUTION ******************************/

  /*
   *  Create a load-balancing object.
   */
  if ((lb_obj = LB_Create_Object(MPI_COMM_WORLD)) == NULL) {
    Gen_Error(0, "fatal:  NULL object returned from LB_Create_Object()\n");
    return 0;
  }

  /* set the method */
  if (LB_Set_Method(lb_obj, prob->method, prob->params) == DLB_FATAL) {
    Gen_Error(0, "fatal:  error returned from LB_Set_Method()\n");
    return 0;
  }

  /* set the tolerance */
  if (LB_Set_Tolerance(lb_obj, prob->tol) == DLB_FATAL) {
    Gen_Error(0, "fatal:  error returned from LB_Set_Tolerance()\n");
    return 0;
  }

  /*
   * set the callback functions
   */
  if (LB_Set_Fn(lb_obj, LB_NUM_OBJ_FN_TYPE, (void *) get_num_elements,
                NULL) == DLB_FATAL) {
    Gen_Error(0, "fatal:  error returned from LB_Set_Fn()\n");
    return 0;
  }

  if (LB_Set_Fn(lb_obj, LB_FIRST_OBJ_FN_TYPE, (void *) get_first_element,
                (void *) *elements) == DLB_FATAL) {
    Gen_Error(0, "fatal:  error returned from LB_Set_Fn()\n");
    return 0;
  }

  if (LB_Set_Fn(lb_obj, LB_NEXT_OBJ_FN_TYPE, (void *) get_next_element,
                (void *) *elements) == DLB_FATAL) {
    Gen_Error(0, "fatal:  error returned from LB_Set_Fn()\n");
    return 0;
  }

  if (LB_Set_Fn(lb_obj, LB_OBJ_WEIGHT_FN_TYPE, (void *) get_elem_weight,
                (void *) *elements) == DLB_FATAL) {
    Gen_Error(0, "fatal:  error returned from LB_Set_Fn()\n");
    return 0;
  }

  /* functions for geometry based algorithms */
  if (prob->read_coord) {
    if (LB_Set_Fn(lb_obj, LB_NUM_GEOM_FN_TYPE, (void *) get_num_geom,
                  NULL) == DLB_FATAL) {
      Gen_Error(0, "fatal:  error returned from LB_Set_Fn()\n");
      return 0;
    }

    if (LB_Set_Fn(lb_obj, LB_GEOM_FN_TYPE, (void *) get_geom,
                  (void *) *elements) == DLB_FATAL) {
      Gen_Error(0, "fatal:  error returned from LB_Set_Fn()\n");
      return 0;
    }
  }

  /*
   * call the load balancer
   */
  if (LB_Balance(lb_obj, &new_decomp, &num_imported, &import_gids,
                 &import_lids, &import_procs, &num_exported, &export_gids,
                 &export_lids, &export_procs) == DLB_FATAL) {
    Gen_Error(0, "fatal:  error returned from LB_Balance()\n");
    return 0;
  }

  /*
   * call another routine to perform the migration
   */
  if (new_decomp) {
    if (!migrate_elements(Proc, elements, lb_obj, num_imported, import_gids,
                          import_lids, import_procs, num_exported, export_gids,
                          export_lids, export_procs)) {
      Gen_Error(0, "fatal:  error returned from migrate_elements()\n");
      return 0;
    }
  }

  (void) LB_Free_Data(&import_gids, &import_lids, &import_procs,
                      &export_gids, &export_lids, &export_procs);


  return 1;

}

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
int get_num_elements(void *data, int *ierr)
{
  *ierr = DLB_OK; /* set error code */

  return(Mesh.num_elems);
}

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
int get_first_element(void *data, LB_GID *global_id, LB_LID *local_id,
                      int *ierr)
{
  ELEM_INFO *elem;

  if (data == NULL) {
    *ierr = DLB_FATAL;
    return 0;
  }
  
  elem = (ELEM_INFO *) data;

  *local_id = 0;
  *global_id = elem[*local_id].globalID;

  *ierr = DLB_OK; /* set error code */

  return 1;
}

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
int get_next_element(void *data, LB_GID global_id, LB_LID local_id,
                     LB_GID *next_global_id, LB_LID *next_local_id, int *ierr)
{
  int found = 0;
  ELEM_INFO *elem;

  if (data == NULL) {
    *ierr = DLB_FATAL;
    return 0;
  }
  
  elem = (ELEM_INFO *) data;

  *next_local_id = local_id + 1;
  if (*next_local_id < Mesh.num_elems) {
    found = 1;
    *next_global_id = elem[*next_local_id].globalID;
  }

  *ierr = DLB_OK; /* set error code */

  return(found);
}

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
double get_elem_weight(void *data, LB_GID global_id,
                       LB_LID local_id, int *ierr)
{
  ELEM_INFO *elem;

  if (data == NULL) {
    *ierr = DLB_FATAL;
    return 0.0;
  }
  
  elem = (ELEM_INFO *) data;

  *ierr = DLB_OK;

  return(elem[local_id].cpu_wgt);
}

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
int get_num_geom(void *data, int *ierr)
{

  *ierr = DLB_OK; /* set error flag */

  return(Mesh.num_dims);
}

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
void get_geom(void *data, LB_GID global_id, LB_LID local_id,
              double *coor, int *ierr)
{
  ELEM_INFO *elem;
  int i, j;
  double tmp;

  if (data == NULL) {
    *ierr = DLB_FATAL;
    return;
  }
  
  elem = (ELEM_INFO *) data;

  /*
   * calculate the geometry of the element by averaging
   * the coordinates of the nodes in its connect table
   */
  for (i = 0; i < Mesh.num_dims; i++) {
    tmp = 0.0;
    for (j = 0; j < Mesh.eb_nnodes[elem[local_id].elem_blk]; j++)
      tmp += elem[local_id].coord[j][i];

    coor[i] = tmp / Mesh.num_dims;
  }

  *ierr = DLB_OK;
}
