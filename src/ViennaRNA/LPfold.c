/*
 * local pair probabilities for RNA secondary structures
 *
 * Stephan Bernhart, Ivo L Hofacker
 * Vienna RNA package
 */
/*
 * todo: compute energy z-score for each window
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>    /* #defines FLT_MAX ... */
#include "ViennaRNA/data_structures.h"
#include "ViennaRNA/utils.h"
#include "ViennaRNA/energy_par.h"
#include "ViennaRNA/fold_vars.h"
#include "ViennaRNA/PS_dot.h"
#include "ViennaRNA/part_func.h"
#include "ViennaRNA/params.h"
#include "ViennaRNA/loop_energies.h"
#include "ViennaRNA/LPfold.h"
#include "ViennaRNA/Lfold.h"


/*
 #################################
 # GLOBAL VARIABLES              #
 #################################
 */

/*
 #################################
 # PRIVATE VARIABLES             #
 #################################
 */

#ifdef  VRNA_BACKWARD_COMPAT

#ifdef _OPENMP
#include <omp.h>
#endif

/* some backward compatibility stuff */
PRIVATE vrna_fold_compound_t  *backward_compat_compound = NULL;
PRIVATE int                   backward_compat           = 0;

#ifdef _OPENMP

#pragma omp threadprivate(backward_compat_compound, backward_compat)

#endif

#endif
/*
 #################################
 # PRIVATE FUNCTION DECLARATIONS #
 #################################
 */

PRIVATE void  GetPtype(vrna_fold_compound_t  *vc,
                            int          j,
                       int          pairsize,
                       const short  *S,
                       int          n);


PRIVATE void  FreeOldArrays(vrna_fold_compound_t  *vc,
                            int i,
                            int ulength);


PRIVATE void  GetNewArrays(vrna_fold_compound_t  *vc,
                            int  j,
                           int  winSize,
                           int ulength);


PRIVATE void  printpbar(vrna_fold_compound_t  *vc,
                        FLT_OR_DBL  **prb,
                        int         winSize,
                        int         i,
                        int         n);


PRIVATE plist *get_deppp(vrna_fold_compound_t  *vc,
                         plist  *pl,
                         int    start,
                         int    pairsize,
                         int    length);


PRIVATE plist *get_plistW(plist       *pl,
                          int         length,
                          int         start,
                          FLT_OR_DBL  **Tpr,
                          int         winSize,
                          float       cutoff,
                          int         *num_p);


PRIVATE void  print_plist(int         length,
                          int         start,
                          FLT_OR_DBL  **Tpr,
                          int         winSize,
                          float       cutoff,
                          FILE        *fp);


PRIVATE void  compute_pU(vrna_fold_compound_t  *vc,
                         int    k,
                         int    ulength,
                         double **pU,
                         int    winSize,
                         int    n,
                         char   *sequence,
                         int    pUoutput);


PRIVATE void  putoutpU(double **pU,
                       int    k,
                       int    ulength,
                       FILE   *fp);


/* PRIVATE void make_ptypes(const short *S, const char *structure); */

PRIVATE void putoutpU_splitup(double  **pUx,
                              int     k,
                              int     ulength,
                              FILE    *fp,
                              char    ident);


PRIVATE void compute_pU_splitup(vrna_fold_compound_t  *vc,
                                int     k,
                                int     ulength,
                                double  **pU,
                                double  **pUO,
                                double  **pUH,
                                double  **pUI,
                                double  **pUM,
                                int     winSize,
                                int     n,
                                char    *sequence,
                                int    pUoutput);


/*
 #################################
 # BEGIN OF FUNCTION DEFINITIONS #
 #################################
 */

PUBLIC plist *
vrna_pf_window(vrna_fold_compound_t *vc,
             double           **pU,
             plist            **dpp2,
             FILE             *pUfp,
             FILE             *spup,
             float            cutoffb)
{
  int         n, m, i, j, k, l, u, u1, type, type_2, tt, ov, do_dpp, simply_putout, noGUclosure, num_p, ulength, pUoutput;
  double      max_real;
  FLT_OR_DBL  temp, Qmax, prm_MLb, prmt, prmt1, qbt1, *tmp, expMLclosing;
  FLT_OR_DBL  *qqm = NULL, *qqm1 = NULL, *qq = NULL, *qq1 = NULL;
  FLT_OR_DBL  *prml = NULL, *prm_l = NULL, *prm_l1 = NULL;
  FLT_OR_DBL  *expMLbase, *scale;


  plist       *dpp, *pl;
  int         split = 0;
  char        *sequence;
  vrna_exp_param_t  *pf_params;
  vrna_md_t         *md;
  short             *S, *S1;
  vrna_mx_pf_t      *matrices;
  int               winSize, pairSize, *rtype;
  FLT_OR_DBL        **q, **qb, **qm, **qm2, **pR, **QI5, **qmb, **q2l;
  char              **ptype;

  ov            = 0;
  Qmax          = 0;
  do_dpp        = 0;
  simply_putout = 0;
  dpp           = NULL;
  pl            = NULL;
  pUoutput      = 0;
  ulength       = 0;

  n = vc->length;
  sequence  = vc->sequence;
  pf_params = vc->exp_params;
  md        = &(pf_params->model_details);
  S1        = vc->sequence_encoding;
  S         = vc->sequence_encoding2;
  matrices  = vc->exp_matrices;
  expMLbase  = matrices->expMLbase;
  scale      = matrices->scale;
  winSize    = vc->window_size;
  pairSize   = md->max_bp_span;
  ptype     = vc->ptype_local;
  rtype     = &(md->rtype[0]);
  expMLclosing  = pf_params->expMLclosing;
  noGUclosure   = md->noGUclosure;


  q         = matrices->q_local;
  qb        = matrices->qb_local;
  qm        = matrices->qm_local;
  qm2       = matrices->qm2_local;
  pR        = matrices->pR;
  QI5       = matrices->QI5;
  qmb       = matrices->qmb;
  q2l       = matrices->q2l;

  if (pU != NULL)
    ulength = (int)pU[0][0] + 0.49;

  if (spup != NULL)
    simply_putout = 1;               /* can't have one without the other */

  if (pUfp != NULL) {
    pUoutput = 1;
  } else if ((pUoutput) && (ulength != 0)) {
    vrna_message_warning("There was a problem with non existing File Pointer for unpaireds, terminating process\n");
    return pl;
  }

  dpp = *dpp2;
  if (dpp != NULL)
    do_dpp = 1;

  /*
   * here, I allocate memory for pU, if has to be saved, I allocate all in one go,
   * if pU is put out and freed, I only allocate what I really need
   */

  qq      = (FLT_OR_DBL *)vrna_alloc(sizeof(FLT_OR_DBL) * (n + 2));
  qq1     = (FLT_OR_DBL *)vrna_alloc(sizeof(FLT_OR_DBL) * (n + 2));
  qqm     = (FLT_OR_DBL *)vrna_alloc(sizeof(FLT_OR_DBL) * (n + 2));
  qqm1    = (FLT_OR_DBL *)vrna_alloc(sizeof(FLT_OR_DBL) * (n + 2));
  prm_l   = (FLT_OR_DBL *)vrna_alloc(sizeof(FLT_OR_DBL) * (n + 2));
  prm_l1  = (FLT_OR_DBL *)vrna_alloc(sizeof(FLT_OR_DBL) * (n + 2));
  prml    = (FLT_OR_DBL *)vrna_alloc(sizeof(FLT_OR_DBL) * (n + 2));

  /* allocate memory and initialize unpaired probabilities */
  if (ulength > 0) {
    if (pUoutput)
      for (i = 1; i <= ulength; i++)
        pU[i] = (double *)vrna_alloc((MAX2(MAXLOOP, ulength) + 2) * sizeof(double));
    else
      for (i = 1; i <= n; i++)
        pU[i] = (double *)vrna_alloc((MAX2(MAXLOOP, ulength) + 2) * sizeof(double));
  }

  if (n < TURN + 2) {
    if (ulength > 0) {
      if (pUoutput) {
        for (i = 1; i <= ulength; i++)
          for (j = 0; j < MAX2(MAXLOOP, ulength) + 1; j++)
            pU[i][j] = 1.;
      } else {
        for (i = 1; i <= n; i++)
          for (j = 0; j < MAX2(MAXLOOP, ulength) + 1; j++)
            pU[i][j] = 1.;
      }
    }

    return pl;
  }


  max_real = (sizeof(FLT_OR_DBL) == sizeof(float)) ? FLT_MAX : DBL_MAX;

  /*  make_ptypes(S, structure); das machmadochlieber lokal, ey! */

  /*
   * array initialization ; qb,qm,q
   * qb,qm,q (i,j) are stored as ((n+1-i)*(n-i) div 2 + n+1-j
   */
  num_p = 0;
  pl    = (plist *)vrna_alloc(1000 * sizeof(plist));


  /* ALWAYS q[i][j] => i>j!! */
  for (j = 1; j < MIN2(TURN + 2, n); j++) {
    /* allocate start */
    GetNewArrays(vc, j, winSize, ulength);
    GetPtype(vc, j, pairSize, S, n);
    for (i = 1; i <= j; i++)
      q[i][j] = scale[(j - i + 1)];
  }
  for (j = TURN + 2; j <= n + winSize; j++) {
    if (j <= n) {
      GetNewArrays(vc, j, winSize, ulength);
      GetPtype(vc, j, pairSize, S, n);
      for (i = MAX2(1, j - winSize); i <= j /* -TURN */; i++)
        q[i][j] = scale[(j - i + 1)];
      for (i = j - TURN - 1; i >= MAX2(1, (j - winSize + 1)); i--) {
        /* construction of partition function of segment i,j */
        /* firstly that given i bound to j : qb(i,j) */
        u     = j - i - 1;
        type  = ptype[i][j];
        if (type != 0) {
          /* hairpin contribution */
          if (((type == 3) || (type == 4)) && noGUclosure)
            qbt1 = 0;
          else
            qbt1 = exp_E_Hairpin(u, type, S1[i + 1], S1[j - 1], sequence + i - 1, pf_params) * scale[u + 2];

          /* interior loops with interior pair k,l */
          for (k = i + 1; k <= MIN2(i + MAXLOOP + 1, j - TURN - 2); k++) {
            u1 = k - i - 1;
            for (l = MAX2(k + TURN + 1, j - 1 - MAXLOOP + u1); l < j; l++) {
              type_2 = ptype[k][l];
              if (type_2) {
                type_2  = rtype[type_2];
                qbt1    += qb[k][l] *
                           exp_E_IntLoop(u1, j - l - 1, type, type_2,
                                         S1[i + 1], S1[j - 1], S1[k - 1], S1[l + 1], pf_params) * scale[k - i + j - l];
              }
            }
          }
          /* multiple stem loop contribution */
          temp = 0.0;
          for (k = i + 2; k <= j - 1; k++)
            temp += qm[i + 1][k - 1] * qqm1[k];
          tt    = rtype[type];
          qbt1  += temp * expMLclosing * exp_E_MLstem(tt, S1[j - 1], S1[i + 1], pf_params) * scale[2];

          qb[i][j] = qbt1;
        } /* end if (type!=0) */
        else {
          qb[i][j] = 0.0;
        }

        /*
         * construction of qqm matrix containing final stem
         * contributions to multiple loop partition function
         * from segment i,j
         */
        qqm[i] = qqm1[i] * expMLbase[1];
        if (type) {
          qbt1    = qb[i][j] * exp_E_MLstem(type, (i > 1) ? S1[i - 1] : -1, (j < n) ? S1[j + 1] : -1, pf_params);
          qqm[i]  += qbt1;
        }

        /*
         * construction of qm matrix containing multiple loop
         * partition function contributions from segment i,j
         */
        temp = 0.0;
        /* ii = my_iindx[i];   ii-k=[i,k-1] */
        /* new qm2 computation done here */
        for (k = i + 1; k <= j; k++)
          temp += (qm[i][k - 1]) * qqm[k];
        if (ulength > 0)
          qm2[i][j] = temp;           /* new qm2 computation done here */

        for (k = i + 1; k <= j; k++)
          temp += expMLbase[k - i] * qqm[k];
        qm[i][j] = (temp + qqm[i]);

        /* auxiliary matrix qq for cubic order q calculation below */
        qbt1 = qb[i][j];
        if (type)
          qbt1 *= exp_E_ExtLoop(type, (i > 1) ? S1[i - 1] : -1, (j < n) ? S1[j + 1] : -1, pf_params);

        qq[i] = qq1[i] * scale[1] + qbt1;

        /* construction of partition function for segment i,j */
        temp = 1.0 * scale[1 + j - i] + qq[i];
        for (k = i; k <= j - 1; k++)
          temp += q[i][k] * qq[k + 1];
        q[i][j] = temp;

        if (temp > Qmax) {
          Qmax = temp;
          if (Qmax > max_real / 10.)
            vrna_message_warning("Q close to overflow: %d %d %g\n", i, j, temp);
        }

        if (temp >= max_real)
          vrna_message_error("overflow in pf_fold while calculating q[%d,%d]\n"
                             "use larger pf_scale", i, j);
      } /* end for i */
      tmp   = qq1;
      qq1   = qq;
      qq    = tmp;
      tmp   = qqm1;
      qqm1  = qqm;
      qqm   = tmp;
    }

    /*
     * just as a general service, I save here the free energy of the windows
     * no output is generated, however,...
     */
    if ((j >= winSize) && (j <= n) && (ulength) && !(pUoutput)) {
      double Fwindow = 0.;
      Fwindow = (-log(q[j - winSize + 1][j]) - winSize * log(pf_params->pf_scale)) * pf_params->kT / 1000.0;

      pU[j][0] = Fwindow;
      /*
       * if (ulength>=winSize)
       * pU[j][winSize]=scale[winSize]/q[j-winSize+1][j];
       */
    }

    if (j > winSize) {
      Qmax = 0;
      /* i=j-winSize; */
      /* initialize multiloopfs */
      for (k = j - winSize; k <= MIN2(n, j); k++) {
        prml[k]   = 0;
        prm_l[k]  = 0;
        /*        prm_l1[k]=0;  others stay */
      }
      prm_l1[j - winSize] = 0;
      k                   = j - winSize;
      for (l = k + TURN + 1; l <= MIN2(n, k + winSize - 1); l++) {
        int a;
        pR[k][l]  = 0; /* set zero at start */
        type      = ptype[k][l];
        if (qb[k][l] == 0)
          continue;

        for (a = MAX2(1, l - winSize + 2); a < MIN2(k, n - winSize + 2); a++)
          pR[k][l] += q[a][k - 1] * q[l + 1][a + winSize - 1] / q[a][a + winSize - 1];

        if (l - k + 1 == winSize) {
          pR[k][l] += 1. / q[k][l];
        } else {
          if (k + winSize - 1 <= n)    /* k outermost */
            pR[k][l] += q[l + 1][k + winSize - 1] / q[k][k + winSize - 1];

          if (l - winSize + 1 >= 1) /* l outermost */
            pR[k][l] += q[l - winSize + 1][k - 1] / q[l - winSize + 1][l];
        }

        pR[k][l] *= exp_E_ExtLoop(type, (k > 1) ? S1[k - 1] : -1, (l < n) ? S1[l + 1] : -1, pf_params);

        type_2  = ptype[k][l];
        type_2  = rtype[type_2];

        for (i = MAX2(MAX2(l - winSize + 1, k - MAXLOOP - 1), 1); i <= k - 1; i++) {
          for (m = l + 1; m <= MIN2(MIN2(l + MAXLOOP - k + i + 2, i + winSize - 1), n); m++) {
            type = ptype[i][m];
            if ((pR[i][m] > 0))
              pR[k][l] += pR[i][m] * exp_E_IntLoop(k - i - 1, m - l - 1, type, type_2,
                                                   S1[i + 1], S1[m - 1], S1[k - 1], S1[l + 1], pf_params) * scale[k - i + m - l];
          }
        }
        if (ulength) {
          /* NOT IF WITHIN INNER LOOP */
          for (i = MAX2(MAX2(l - winSize + 1, k - MAXLOOP - 1), 1); i <= k - 1; i++) {
            for (m = l + 1; m <= MIN2(MIN2(l + MAXLOOP - k + i + 2, i + winSize - 1), n); m++) {
              type = ptype[i][m];
              if ((pR[i][m] > 0)) {
                temp = pR[i][m] * qb[k][l] * exp_E_IntLoop(k - i - 1, m - l - 1, type, type_2,
                                                           S1[i + 1], S1[m - 1], S1[k - 1], S1[l + 1], pf_params) * scale[k - i + m - l];
                QI5[l][m - l - 1] += temp;
                QI5[i][k - i - 1] += temp;
              }
            }
          }
        }
      }
      /* 3. bonding k,l as substem of multi-loop enclosed by i,m */
      prm_MLb = 0.;
      if (k > 1) {
        /* sonst nix! */
        for (l = MIN2(n - 1, k + winSize - 2); l >= k + TURN + 1; l--) {
          /* opposite direction */
          m     = l + 1;
          prmt  = prmt1 = 0.0;
          tt    = ptype[k - 1][m];
          tt    = rtype[tt];
          prmt1 = pR[k - 1][m] *expMLclosing *exp_E_MLstem(tt,
                                                           S1[l],
                                                           S1[k],
                                                           pf_params);


          for (i = MAX2(1, l - winSize + 2); i < k - 1 /* TURN */; i++) {
            tt    = ptype[i][m];
            tt    = rtype[tt];
            prmt  += pR[i][m] * exp_E_MLstem(tt, S1[m - 1], S1[i + 1], pf_params) * qm[i + 1][k - 1];
          }
          tt        = ptype[k][l];
          prmt      *= expMLclosing;
          prml[m]   = prmt;
          prm_l[m]  = prm_l1[m] * expMLbase[1] + prmt1;

          prm_MLb = prm_MLb * expMLbase[1] + prml[m];
          /*
           * same as:    prm_MLb = 0;
           * for (i=n; i>k; i--)  prm_MLb += prml[i]*expMLbase[k-i-1];
           */
          prml[m] = prml[m] + prm_l[m];

          if (qb[k][l] == 0.)
            continue;

          temp = prm_MLb;

          if (ulength) {
            double dang;
            /* coefficient for computations of unpairedarrays */
            dang = qb[k][l] * exp_E_MLstem(tt, S1[k - 1], S1[l + 1], pf_params) * scale[2];
            for (m = MIN2(k + winSize - 2, n); m >= l + 2; m--) {
              qmb[l][m - l - 1] += prml[m] * dang;
              q2l[l][m - l - 1] += (prml[m] - prm_l[m]) * dang;
            }
          }

          for (m = MIN2(k + winSize - 2, n); m >= l + 2; m--)
            temp += prml[m] * qm[l + 1][m - 1];

          temp      *= exp_E_MLstem(tt, (k > 1) ? S1[k - 1] : -1, (l < n) ? S1[l + 1] : -1, pf_params) * scale[2];
          pR[k][l]  += temp;

          if (pR[k][l] > Qmax) {
            Qmax = pR[k][l];
            if (Qmax > max_real / 10.)
              vrna_message_warning("P close to overflow: %d %d %g %g\n",
                                   i, m, pR[k][l], qb[k][l]);
          }

          if (pR[k][l] >= max_real) {
            ov++;
            pR[k][l] = FLT_MAX;
          }
        } /* end for (l=..) */
      }

      tmp     = prm_l1;
      prm_l1  = prm_l;
      prm_l   = tmp;

      /* end for (l=..)   */
      if ((ulength) && (k - MAXLOOP - 1 > 0)) {
        /* if (pUoutput) pU[k-MAXLOOP-1]=(double *)vrna_alloc((ulength+2)*sizeof(double)); */
        if (split) {
          /* generate the new arrays, if you want them somewhere else, you have to generate them and overgive them ;) */
          double  **pUO;
          double  **pUI;
          double  **pUM;
          double  **pUH;
          pUO = (double **)vrna_alloc((n + 1) * sizeof(double *));
          pUI = (double **)vrna_alloc((n + 1) * sizeof(double *));
          pUM = (double **)vrna_alloc((n + 1) * sizeof(double *));
          pUH = (double **)vrna_alloc((n + 1) * sizeof(double *));
          if (pUoutput) {
            for (i = 1; i <= ulength; i++) {
              pUH[i]  = (double *)vrna_alloc((MAX2(MAXLOOP, ulength) + 2) * sizeof(double));
              pUI[i]  = (double *)vrna_alloc((MAX2(MAXLOOP, ulength) + 2) * sizeof(double));
              pUO[i]  = (double *)vrna_alloc((MAX2(MAXLOOP, ulength) + 2) * sizeof(double));
              pUM[i]  = (double *)vrna_alloc((MAX2(MAXLOOP, ulength) + 2) * sizeof(double));
            }
          }

          //dont want to have that yet?
          /*
           *  else {
           * for (i=1; i<=n; i++) pU[i]=(double *)vrna_alloc((MAX2(MAXLOOP,ulength)+2)*sizeof(double));
           * }
           */
          compute_pU_splitup(vc, k - MAXLOOP - 1, ulength, pU, pUO, pUH, pUI, pUM, winSize, n, sequence, pUoutput);
          if (pUoutput) {
            putoutpU_splitup(pUO, k - MAXLOOP - 1, ulength, pUfp, 'E');
            putoutpU_splitup(pUH, k - MAXLOOP - 1, ulength, pUfp, 'H');
            putoutpU_splitup(pUI, k - MAXLOOP - 1, ulength, pUfp, 'I');
            putoutpU_splitup(pUM, k - MAXLOOP - 1, ulength, pUfp, 'M');
          }
        } else {
          compute_pU(vc, k - MAXLOOP - 1, ulength, pU, winSize, n, sequence, pUoutput);

          /* here, we put out and free pUs not in use any more (hopefully) */
          if (pUoutput)
            putoutpU(pU, k - MAXLOOP - 1, ulength, pUfp);
        }
      }

      if (j - (2 * winSize + MAXLOOP + 1) > 0) {
        printpbar(vc, pR, winSize, j - (2 * winSize + MAXLOOP + 1), n);
        if (simply_putout)
          print_plist(n, j - (2 * winSize + MAXLOOP + 1), pR, winSize, cutoffb, spup);
        else
          pl = get_plistW(pl, n, j - (2 * winSize + MAXLOOP + 1), pR, winSize, cutoffb, &num_p);

        if (do_dpp)
          dpp = get_deppp(vc, dpp, j - (2 * winSize - MAXLOOP), pairSize, n);

        FreeOldArrays(vc, j - (2 * winSize + MAXLOOP + 1), ulength);
      }
    }   /* end if (do_backtrack) */
  }/* end for j */

  /* finish output and free */
  for (j = MAX2(1, n - MAXLOOP); j <= n; j++) {
    /* if (pUoutput) pU[j]=(double *)vrna_alloc((ulength+2)*sizeof(double)); */
    if (ulength)
      compute_pU(vc, j, ulength, pU, winSize, n, sequence, pUoutput);

    /* here, we put out and free pUs not in use any more (hopefully) */
    if (pUoutput)
      putoutpU(pU, j, ulength, pUfp);
  }
  for (j = MAX2(n - winSize - MAXLOOP, 1); j <= n; j++) {
    printpbar(vc, pR, winSize, j, n);
    if (simply_putout)
      print_plist(n, j, pR, winSize, cutoffb, spup);
    else
      pl = get_plistW(pl, n, j, pR, winSize, cutoffb, &num_p);

    if ((do_dpp) && j < n)
      dpp = get_deppp(vc, dpp, j, pairSize, n);

    FreeOldArrays(vc, j, ulength);
  }
  /* free_pf_arrays_L(); */
  free(S);
  free(S1);
  S = S1 = NULL;
  if (ov > 0)
    vrna_message_warning("%d overflows occurred while backtracking;\n"
                         "you might try a smaller pf_scale than %g\n",
                         ov, pf_params->pf_scale);

  free(qq);
  free(qq1);
  free(qqm);
  free(qqm1);
  free(prm_l);
  free(prm_l1);
  free(prml);

  *dpp2 = dpp;

  return pl;
}


PRIVATE void
printpbar(vrna_fold_compound_t  *vc,
          FLT_OR_DBL  **prb,
          int         winSize,
          int         i,
          int         n)
{
  int j;
  int howoften = 0; /* how many samples do we have for this pair */
  int pairdist;
  FLT_OR_DBL  **qb;
  
  qb = vc->exp_matrices->qb_local;

  for (j = i + TURN; j < MIN2(i + winSize, n + 1); j++) {
    pairdist = (j - i + 1);
    /* 4cases */
    howoften  = MIN2(winSize - pairdist + 1, i);  /* pairdist,start */
    howoften  = MIN2(howoften, n - j + 1);        /* end */
    howoften  = MIN2(howoften, n - winSize + 1);  /* windowsize */
    prb[i][j] *= qb[i][j] / howoften;
  }
  return;
}


PRIVATE void
FreeOldArrays(vrna_fold_compound_t  *vc,
              int i,
              int ulength)
{
  FLT_OR_DBL    **pR, **q, **qb, **qm, **qm2, **QI5, **qmb, **q2l;
  char          **ptype;
  vrna_mx_pf_t  *mx;

  mx = vc->exp_matrices;
  pR  = mx->pR;
  q   = mx->q_local;
  qb  = mx->qb_local;
  qm  = mx->qm_local;
  qm2 = mx->qm2_local;
  QI5 = mx->QI5;
  qmb = mx->qmb;
  q2l = mx->q2l;
  ptype = vc->ptype_local;

  /* free arrays no longer needed */
  free(pR[i] + i);
  free(q[i] + i);
  free(qb[i] + i);
  free(qm[i] + i);
  if (ulength != 0) {
    free(qm2[i] + i);
    free(QI5[i]);
    free(qmb[i]);
    free(q2l[i]);
  }

  free(ptype[i] + i);
  return;
}


PRIVATE void
GetNewArrays(vrna_fold_compound_t  *vc,
              int  j,
             int  winSize,
             int ulength)
{
  FLT_OR_DBL    **pR, **q, **qb, **qm, **qm2, **QI5, **qmb, **q2l;
  char          **ptype;
  vrna_mx_pf_t  *mx;

  mx = vc->exp_matrices;
  pR  = mx->pR;
  q   = mx->q_local;
  qb  = mx->qb_local;
  qm  = mx->qm_local;
  qm2 = mx->qm2_local;
  QI5 = mx->QI5;
  qmb = mx->qmb;
  q2l = mx->q2l;
  ptype = vc->ptype_local;

  /* allocate new part of arrays */
  pR[j] = (FLT_OR_DBL *)vrna_alloc((winSize + 1) * sizeof(FLT_OR_DBL));
  pR[j] -= j;
  q[j]  = (FLT_OR_DBL *)vrna_alloc((winSize + 1) * sizeof(FLT_OR_DBL));
  q[j]  -= j;
  qb[j] = (FLT_OR_DBL *)vrna_alloc((winSize + 1) * sizeof(FLT_OR_DBL));
  qb[j] -= j;
  qm[j] = (FLT_OR_DBL *)vrna_alloc((winSize + 1) * sizeof(FLT_OR_DBL));
  qm[j] -= j;
  if (ulength != 0) {
    qm2[j]  = (FLT_OR_DBL *)vrna_alloc((winSize + 1) * sizeof(FLT_OR_DBL));
    qm2[j]  -= j;
    QI5[j]  = (FLT_OR_DBL *)vrna_alloc((winSize + 1) * sizeof(FLT_OR_DBL));
    qmb[j]  = (FLT_OR_DBL *)vrna_alloc((winSize + 1) * sizeof(FLT_OR_DBL));
    q2l[j]  = (FLT_OR_DBL *)vrna_alloc((winSize + 1) * sizeof(FLT_OR_DBL));
  }

  ptype[j]  = (char *)vrna_alloc((winSize + 1) * sizeof(char));
  ptype[j]  -= j;
  return;
}


PRIVATE void
GetPtype( vrna_fold_compound_t *vc,
          int          i,
          int          winSize,
          const short  *S,
           int          n)
{
  /* make new entries in ptype array */
  int       j;
  int       type;
  char      **ptype;
  vrna_md_t *md;

  ptype = vc->ptype_local;
  md    = &(vc->exp_params->model_details);

  for (j = i; j <= MIN2(i + winSize, n); j++) {
    type        = md->pair[S[i]][S[j]];
    ptype[i][j] = (char)type;
  }
  return;
}


PRIVATE plist *
get_plistW(plist      *pl,
           int        length,
           int        start,
           FLT_OR_DBL **Tpr,
           int        winSize,
           float      cutoff,
           int        *num_p)
{
  /* get pair probibilities out of pr array */
  int j, max_p;

  max_p = 1000;
  while (max_p < (*num_p))
    max_p *= 2;

  for (j = start + 1; j <= MIN2(start + winSize, length); j++) {
    if (Tpr[start][j] < cutoff)
      continue;

    if ((*num_p) == max_p - 1) {
      max_p *= 2;
      pl    = (plist *)vrna_realloc(pl, max_p * sizeof(plist));
    }

    pl[(*num_p)].i   = start;
    pl[(*num_p)].j   = j;
    pl[(*num_p)++].p = Tpr[start][j];
  }

  /* mark end of data with zeroes */
  pl[(*num_p)].i = 0;
  pl[(*num_p)].j = 0;
  pl[(*num_p)].p = 0.;
  /* pl=(plist *)vrna_realloc(pl,(count)*sizeof(plist)); */
  return pl;
}


PRIVATE plist *
get_deppp(vrna_fold_compound_t  *vc,
          plist *pl,
          int   start,
          int   pairsize,
          int   length)
{
  /* compute dependent pair probabilities */
  int     i, j, count = 0;
  double  tmp;
  plist   *temp;
  char    **ptype;
  short   *S1;
  FLT_OR_DBL  **qb, *scale;
  int         *rtype;
  vrna_exp_param_t *pf_params;

  S1    = vc->sequence_encoding;
  pf_params = vc->exp_params;
  ptype = vc->ptype_local;
  qb  = vc->exp_matrices->qb_local;
  scale = vc->exp_matrices->scale;
  rtype = &(pf_params->model_details.rtype[0]);

  temp = (plist *)vrna_alloc(pairsize * sizeof(plist)); /* holds temporary deppp */
  for (j = start + TURN; j < MIN2(start + pairsize, length); j++) {
    if ((qb[start][j] * qb[start - 1][(j + 1)]) > 10e-200) {
      int type    = ptype[start - 1][j + 1];
      int type_2  = rtype[(unsigned char)ptype[start][j]];
      tmp = qb[start][j] / qb[start - 1][(j + 1)] * exp_E_IntLoop(0, 0, type, type_2,
                                                                  S1[start], S1[j], S1[start - 1], S1[j + 1], pf_params) * scale[2];
      temp[count].i   = start;
      temp[count].j   = j;
      temp[count++].p = tmp;
    }
  }
  /* write it to list of deppps */
  for (i = 0; pl[i].i != 0; i++) ;
  pl = (plist *)vrna_realloc(pl, (i + count + 1) * sizeof(plist));
  for (j = 0; j < count; j++) {
    pl[i + j].i = temp[j].i;
    pl[i + j].j = temp[j].j;
    pl[i + j].p = temp[j].p;
  }
  pl[i + count].i = 0;
  pl[i + count].j = 0;
  pl[i + count].p = 0;
  free(temp);
  return pl;
}


PRIVATE void
print_plist(int         length,
            int         start,
            FLT_OR_DBL  **Tpr,
            int         winSize,
            float       cutoff,
            FILE        *fp)
{
  /* print out of pr array, do not save */
  int j;


  for (j = start + 1; j <= MIN2(start + winSize, length); j++) {
    if (Tpr[start][j] < cutoff)
      continue;

    fprintf(fp, "%d  %d  %g\n", start, j, Tpr[start][j]);
  }

  /* mark end of data with zeroes */

  return;
}


PRIVATE void
compute_pU(vrna_fold_compound_t *vc,
           int    k,
           int    ulength,
           double **pU,
           int    winSize,
           int    n,
           char   *sequence,
           int    pUoutput)
{
  /*
   *  here, we try to add a function computing all unpaired probabilities starting at some i,
   *  going down to $unpaired, to be unpaired, i.e. a list with entries from 1 to unpaired for
   *  every i, with the probability of a stretch of length x, starting at i-x+1, to be unpaired
   */
  int         startu;
  int         i5;
  int         j3, len, obp, *rtype;
  double      temp;
  double      *QBE;
  FLT_OR_DBL  expMLclosing;
  FLT_OR_DBL  *expMLbase, **qm2, **qm, *scale, **pR, **QI5, **q2l, **q, **qmb;
  char        **ptype;
  vrna_exp_param_t  *pf_params;
  short       *S1;

  S1        = vc->sequence_encoding;
  pf_params = vc->exp_params;
  ptype     = vc->ptype_local;
  rtype     = &(pf_params->model_details.rtype[0]);
  scale     = vc->exp_matrices->scale;
  q         = vc->exp_matrices->q_local;
  qm        = vc->exp_matrices->qm_local;
  qm2       = vc->exp_matrices->qm2_local;
  expMLbase = vc->exp_matrices->expMLbase;
  expMLclosing = pf_params->expMLclosing;
  pR            = vc->exp_matrices->pR;
  QI5           = vc->exp_matrices->QI5;
  q2l           = vc->exp_matrices->q2l;
  qmb        = vc->exp_matrices->qmb;

  QBE = (double *)vrna_alloc((MAX2(ulength, MAXLOOP) + 2) * sizeof(double));

  /* first, we will */
  /* for k<=ulength, pU[k][k]=0, because no bp can enclose it */
  if (pUoutput && k + ulength <= n)
    pU[k + ulength] = (double *)vrna_alloc((ulength + 2) * sizeof(double));

  /*compute pu[k+ulength][ulength] */
  for (i5 = MAX2(k + ulength - winSize + 1, 1); i5 <= k; i5++) {
    for (j3 = k + ulength + 1; j3 <= MIN2(n, i5 + winSize - 1); j3++) {
      /*
       *  if (k>400) {
       * printf("i%d j%d  ",i5,j3);
       * fflush(stdout);
       * }
       */
      if (ptype[i5][j3] != 0) {
        /*
         * (.. >-----|..........)
         * i5  j     j+ulength  j3
         */
        /* Multiloops */
        temp = (i5 < k) ? qm2[i5 + 1][k] * expMLbase[j3 - k - 1] : 0.; /* (..{}{}-----|......) */

        if (j3 - 1 > k + ulength)
          temp += qm2[k + ulength + 1][j3 - 1] * expMLbase[k + ulength - i5]; /* (..|-----|{}{}) */

        if ((i5 < k) && (j3 - 1 > k + ulength))
          temp += qm[i5 + 1][k] * qm[k + ulength + 1][j3 - 1] * expMLbase[ulength]; /* ({}|-----|{}) */

        /* add dangles, multloopclosing etc. */
        temp *= exp_E_MLstem(rtype[(unsigned char)ptype[i5][j3]], S1[j3 - 1], S1[i5 + 1], pf_params) * scale[2] * expMLclosing;
        /* add hairpins */
        temp += exp_E_Hairpin(j3 - i5 - 1, ptype[i5][j3], S1[i5 + 1], S1[j3 - 1], sequence + i5 - 1, pf_params) * scale[j3 - i5 + 1];
        /* add outer probability */
        temp                      *= pR[i5][j3];
        pU[k + ulength][ulength]  += temp;
      }
    }
  }
  /* code doubling to avoid if within loop */
#if 0
  /* initialization for interior loops,
   * it is not recomended to have verysmall ulengths!!
   */
  if (ulength < MAXLOOP) {
    int k5;
    int l3;
    int outype;
    /* kl bp is 5' */
    /*
     * MAXLOOP>((l5-k5-1)+(j3-l3-1)
     * k-winSize+ulength<i5<k-TURN-1;
     * k+ulength<j3<=k+MAXLOOP+1
     * if i then use l3, it is easier by far:
     * j3-MAXLOOP<=l3<=k
     * i5<k5<k-TURN k5<=i5+l3+2+MAXLOOP-j3
     * k5+TURN<l3<=k
     */
    for (i5 = MAX2(k + ulength - winSize, 1); i5 < k - TURN - 1; i5++) {
      for (j3 = k + ulength + 1; j3 <= MIN2(n, MIN2(i5 + winSize - 1, k + MAXLOOP + 1)); j3++) {
        double temp = 0;
        if (outype = ptype[i5][j3] > 0) /* oder so halt */
          for (l3 = MAX2(i5 + TURN + 1, j3 - MAXLOOP - 1); l3 <= k; l3++) {
            for (k5 = i5 + 1; k5 <= MIN2(l3 - TURN - 1, MAXLOOP + i5 + l3 + 2 - j3); k5++)
              if (ptype[k5][l3])
                temp += qb[k5][l3] * expLoopEnergy(k5 - i5 - 1, j3 - l3 - 1, outype, rtype[ptype[k5][l3]], S1[i5 + 1], S1[j3 - 1], S1[k5 - 1], S1[l3 + 1]);
          }

        temp                      *= pR[i5][j3];
        pU[k + ulength][ulength]  += temp;
      }
    }
    /* kl bp is 3' */
    /*
     * k+ulength-MAXLOOP<=i5<=k
     * k+ulength+1+TURN<j3<i5+winSize
     * k+ulength+1<=k5<i5+MAXLOOP+2 || k5<j3-TURN
     * k5<l3<j3 || j3-k5-i5-2-ML<=l3<j3
     */
    for (i5 = MAX2(1, MAX2(k + ulength - winSize, k + ulength - MAXLOOP)); i5 <= k; i5++) {
      for (j3 = k + ulength + TURN + 2; j3 < MIN2(n + 1, i5 + winSize); j3++) {
        double temp = 0;
        if (outype = ptype[i5][j3] > 0) /* oder so halt */
          for (k5 = k + ulength + 1; k5 < MIN2(j3 - TURN - 1, i5 + MAXLOOP + 2); k5++) {
            for (l3 = MAX2(k5 + TURN + 1, j3 + k5 - i5 - 2 - MAXLOOP); l3 < j3; l3++)
              if (ptype[k5][l3])
                temp += qb[k5][l3] * expLoopEnergy(k5 - i5 - 1, j3 - l3 - 1, outype, rtype[ptype[k5][l3]], S1[i5 + 1], S1[j3 - 1], S1[k5 - 1], S1[l3 + 1]);
          }

        temp                      *= pR[i5][j3];
        pU[k + ulength][ulength]  += temp;
      }
    }
  }

  /* Add up Is QI5[l][m-l-1] QI3 */
  /* Add up Interior loop terms */
  temp = 0.;

  for (len = winSize; len >= ulength; len--)
    temp += QI3[k][len];
  for (; len > 0; len--) {
    temp      += QI3[k][len];
    QBE[len]  += temp;
  }
#endif
  temp = 0.;
  for (len = winSize; len >= MAX2(ulength, MAXLOOP); len--)
    temp += QI5[k][len];
  for (; len > 0; len--) {
    temp      += QI5[k][len];
    QBE[len]  += temp; /* replace QBE with QI */
  }
  /* Add Hairpinenergy to QBE */
  temp = 0.;
  for (obp = MIN2(n, k + winSize - 1); obp > k + ulength; obp--)
    if (ptype[k][obp])
      temp += pR[k][obp] * exp_E_Hairpin(obp - k - 1, ptype[k][obp], S1[k + 1], S1[obp - 1], sequence + k - 1, pf_params) * scale[obp - k + 1];

  for (obp = MIN2(n, MIN2(k + winSize - 1, k + ulength)); obp > k + 1; obp--) {
    if (ptype[k][obp])
      temp += pR[k][obp] * exp_E_Hairpin(obp - k - 1, ptype[k][obp], S1[k + 1], S1[obp - 1], sequence + k - 1, pf_params) * scale[obp - k + 1];

    QBE[obp - k - 1] += temp;  /* add hairpins to QBE (all in one array) */
  }
  /* doubling the code to get the if out of the loop */

  /*
   * Add up Multiloopterms  qmb[l][m]+=prml[m]*dang;
   * q2l[l][m]+=(prml[m]-prm_l[m])*dang;
   */

  temp = 0.;
  for (len = winSize; len >= ulength; len--)
    temp += q2l[k][len] * expMLbase[len];
  for (; len > 0; len--) {
    temp      += q2l[k][len] * expMLbase[len];
    QBE[len]  += temp; /* add (()()____) type cont. to I3 */
  }
  for (len = 1; len < ulength; len++) {
    for (obp = k + len + TURN; obp <= MIN2(n, k + winSize - 1); obp++)
      /* add (()___()) */
      QBE[len] += qmb[k][obp - k - 1] * qm[k + len + 1 /*2*/][obp - 1] * expMLbase[len];
  }
  for (len = 1; len < ulength; len++) {
    for (obp = k + len + TURN + TURN; obp <= MIN2(n, k + winSize - 1); obp++) {
      if (ptype[k][obp]) {
        temp      = exp_E_MLstem(rtype[(unsigned char)ptype[k][obp]], S1[obp - 1], S1[k + 1], pf_params) * scale[2] * expMLbase[len] * expMLclosing;  /* k:obp */
        QBE[len]  += pR[k][obp] * temp * qm2[k + len + 1][obp - 1];                                                                                   /* add (___()()) */
      }
    }
  }
  /*
   * After computing all these contributions in QBE[len], that k is paired
   * and the unpaired stretch is AT LEAST len long, we start to add that to
   * the old unpaired thingies;
   */
  for (len = 1; len < MIN2(MAX2(ulength, MAXLOOP), n - k); len++)
    pU[k + len][len] += pU[k + len][len + 1] + QBE[len];

  /* open chain */
  if ((ulength >= winSize) && (k >= ulength))
    pU[k][winSize] = scale[winSize] / q[k - winSize + 1][k];

  /*
   * now the not enclosed by any base pair terms for whatever it is we do not need anymore...
   * ... which should be e.g; k, again
   */
  for (startu = MIN2(ulength, k); startu > 0; startu--) {
    temp = 0.;
    for (i5 = MAX2(1, k - winSize + 2); i5 <= MIN2(k - startu, n - winSize + 1); i5++)
      temp += q[i5][k - startu] * q[k + 1][i5 + winSize - 1] * scale[startu] / q[i5][i5 + winSize - 1];
    /* the 2 Cases where the borders are on the edge of the interval */
    if ((k >= winSize) && (startu + 1 <= winSize))
      temp += q[k - winSize + 1][k - startu] * scale[startu] / q[k - winSize + 1][k];

    if ((k <= n - winSize + startu) && (k - startu >= 0) && (k < n) && (startu + 1 <= winSize))
      temp += q[k + 1][k - startu + winSize] * scale[startu] / q[k - startu + 1][k - startu + winSize];

    /* Divide by number of possible windows */
    pU[k][startu] += temp;
    {
      int leftmost, rightmost;

      leftmost      = MAX2(1, k - winSize + 1);
      rightmost     = MIN2(n - winSize + 1, k - startu + 1);
      pU[k][startu] /= (rightmost - leftmost + 1);
    }
  }
  free(QBE);
  return;
}


PRIVATE void
putoutpU(double **pUx,
         int    k,
         int    ulength,
         FILE   *fp)
{
  /* put out unpaireds for k, and free pU[k], make sure we don't need pU[k] any more!! */
  /* could use that for hairpins, also! */
  int i;

  fprintf(fp, "%d\t", k);
  for (i = 1; i <= MIN2(ulength, k); i++)
    fprintf(fp, "%.5g\t", pUx[k][i]);
  fprintf(fp, "\n");
  free(pUx[k]);
}


PRIVATE void
putoutpU_splitup(double **pUx,
                 int    k,
                 int    ulength,
                 FILE   *fp,
                 char   ident)
{
  /* put out unpaireds for k, and free pU[k], make sure we don't need pU[k] any more!! */
  /* could use that for hairpins, also! */
  int i;

  fprintf(fp, "%d\t", k);
  for (i = 1; i <= MIN2(ulength, k); i++)
    fprintf(fp, "%.5g\t", pUx[k][i]);
  fprintf(fp, "\t%c\n", ident);
  free(pUx[k]);
}


/*
 * Here: Space for questions...
 */
PRIVATE void
compute_pU_splitup(vrna_fold_compound_t *vc,
                   int    k,
                   int    ulength,
                   double **pU,
                   double **pUO,
                   double **pUH,
                   double **pUI,
                   double **pUM,
                   int    winSize,
                   int    n,
                   char   *sequence,
                   int    pUoutput)
{
  /*
   *  here, we try to add a function computing all unpaired probabilities starting at some i,
   *  going down to $unpaired, to be unpaired, i.e. a list with entries from 1 to unpaired for
   *  every i, with the probability of a stretch of length x, starting at i-x+1, to be unpaired
   */
  int         startu;
  int         i5;
  int         j3, len, obp, *rtype;
  double      temp;
  double      *QBE;
  double      *QBI;
  double      *QBM;
  double      *QBH;

  FLT_OR_DBL  expMLclosing, *expMLbase, **q, **qm, **qm2, *scale, **pR, **QI5, **q2l, **qmb;
  vrna_exp_param_t  *pf_params;
  char              **ptype;
  short             *S1;

  pf_params     = vc->exp_params;
  ptype         = vc->ptype_local;
  rtype         = &(pf_params->model_details.rtype[0]);

  expMLclosing = pf_params->expMLclosing;
  expMLbase     = vc->exp_matrices->expMLbase;
  scale         = vc->exp_matrices->scale;
  q             = vc->exp_matrices->q_local;
  qm            = vc->exp_matrices->qm_local;
  qm2            = vc->exp_matrices->qm2_local;
  qmb            = vc->exp_matrices->qmb;
  q2l            = vc->exp_matrices->q2l;
  QI5            = vc->exp_matrices->QI5;
  pR             = vc->exp_matrices->pR;

  QBE = (double *)vrna_alloc((MAX2(ulength, MAXLOOP) + 2) * sizeof(double));
  QBM = (double *)vrna_alloc((MAX2(ulength, MAXLOOP) + 2) * sizeof(double));
  QBI = (double *)vrna_alloc((MAX2(ulength, MAXLOOP) + 2) * sizeof(double));
  QBH = (double *)vrna_alloc((MAX2(ulength, MAXLOOP) + 2) * sizeof(double));

  /* first, we will */
  /* for k<=ulength, pU[k][k]=0, because no bp can enclose it */
  if (pUoutput && k + ulength <= n)
    pU[k + ulength] = (double *)vrna_alloc((ulength + 2) * sizeof(double));

  /* compute pu[k+ulength][ulength] */
  for (i5 = MAX2(k + ulength - winSize + 1, 1); i5 <= k; i5++) {
    for (j3 = k + ulength + 1; j3 <= MIN2(n, i5 + winSize - 1); j3++) {
      /*
       *  if (k>400) {
       * printf("i%d j%d  ",i5,j3);
       * fflush(stdout);
       * }
       */
      if (ptype[i5][j3] != 0) {
        /*
         * (.. >-----|..........)
         * i5  j     j+ulength  j3
         */
        /* Multiloops */
        temp = (i5 < k) ? qm2[i5 + 1][k] * expMLbase[j3 - k - 1] : 0.; /* (..{}{}-----|......) */

        if (j3 - 1 > k + ulength)
          temp += qm2[k + ulength + 1][j3 - 1] * expMLbase[k + ulength - i5]; /* (..|-----|{}{}) */

        if ((i5 < k) && (j3 - 1 > k + ulength))
          temp += qm[i5 + 1][k] * qm[k + ulength + 1][j3 - 1] * expMLbase[ulength]; /* ({}|-----|{}) */

        /* add dangles, multloopclosing etc. */
        temp *= exp_E_MLstem(rtype[(unsigned char)ptype[i5][j3]], S1[j3 - 1], S1[i5 + 1], pf_params) * scale[2] * expMLclosing;
        /* add hairpins */
        temp += exp_E_Hairpin(j3 - i5 - 1, ptype[i5][j3], S1[i5 + 1], S1[j3 - 1], sequence + i5 - 1, pf_params) * scale[j3 - i5 + 1];
        /* add outer probability */
        temp                      *= pR[i5][j3];
        pU[k + ulength][ulength]  += temp;
      }
    }
  }
  /* code doubling to avoid if within loop */
  temp = 0.;
  for (len = winSize; len >= MAX2(ulength, MAXLOOP); len--)
    temp += QI5[k][len];
  for (; len > 0; len--) {
    temp      += QI5[k][len];
    QBI[len]  += temp;
    QBE[len]  += temp; /* replace QBE with QI */
  }
  /* Add Hairpinenergy to QBE */
  temp = 0.;
  for (obp = MIN2(n, k + winSize - 1); obp > k + ulength; obp--)
    if (ptype[k][obp])
      temp += pR[k][obp] * exp_E_Hairpin(obp - k - 1, ptype[k][obp], S1[k + 1], S1[obp - 1], sequence + k - 1, pf_params) * scale[obp - k + 1];

  for (obp = MIN2(n, MIN2(k + winSize - 1, k + ulength)); obp > k + 1; obp--) {
    if (ptype[k][obp])
      temp += pR[k][obp] * exp_E_Hairpin(obp - k - 1, ptype[k][obp], S1[k + 1], S1[obp - 1], sequence + k - 1, pf_params) * scale[obp - k + 1];

    QBH[obp - k - 1]  += temp;
    QBE[obp - k - 1]  += temp; /* add hairpins to QBE (all in one array) */
  }
  /* doubling the code to get the if out of the loop */

  /*
   * Add up Multiloopterms  qmb[l][m]+=prml[m]*dang;
   * q2l[l][m]+=(prml[m]-prm_l[m])*dang;
   */

  temp = 0.;
  for (len = winSize; len >= ulength; len--)
    temp += q2l[k][len] * expMLbase[len];
  for (; len > 0; len--) {
    temp      += q2l[k][len] * expMLbase[len];
    QBM[len]  += temp;
    QBE[len]  += temp; /* add (()()____) type cont. to I3 */
  }
  for (len = 1; len < ulength; len++) {
    for (obp = k + len + TURN; obp <= MIN2(n, k + winSize - 1); obp++) {
      /* add (()___()) */
      QBM[len]  += qmb[k][obp - k - 1] * qm[k + len + 1 /*2*/][obp - 1] * expMLbase[len];
      QBE[len]  += qmb[k][obp - k - 1] * qm[k + len + 1 /*2*/][obp - 1] * expMLbase[len];
    }
  }
  for (len = 1; len < ulength; len++) {
    for (obp = k + len + TURN + TURN; obp <= MIN2(n, k + winSize - 1); obp++) {
      if (ptype[k][obp]) {
        temp      = exp_E_MLstem(rtype[(unsigned char)ptype[k][obp]], S1[obp - 1], S1[k + 1], pf_params) * scale[2] * expMLbase[len] * expMLclosing;  /* k:obp */
        QBE[len]  += pR[k][obp] * temp * qm2[k + len + 1][obp - 1];                                                                                   /* add (___()()) */
        QBM[len]  += pR[k][obp] * temp * qm2[k + len + 1][obp - 1];                                                                                   /* add (___()()) */
      }
    }
  }
  /*
   * After computing all these contributions in QBE[len], that k is paired
   * and the unpaired stretch is AT LEAST len long, we start to add that to
   * the old unpaired thingies;
   */
  for (len = 1; len < MIN2(MAX2(ulength, MAXLOOP), n - k); len++) {
    pU[k + len][len]  += pU[k + len][len + 1] + QBE[len];
    pUH[k + len][len] += pUH[k + len][len + 1] + QBH[len];
    pUM[k + len][len] += pUM[k + len][len + 1] + QBM[len];
    pUI[k + len][len] += pUI[k + len][len + 1] + QBI[len];
  }

  /* open chain */
  if ((ulength >= winSize) && (k >= ulength))
    pUO[k][winSize] = scale[winSize] / q[k - winSize + 1][k];

  /* open chain */
  if ((ulength >= winSize) && (k >= ulength))
    pU[k][winSize] = scale[winSize] / q[k - winSize + 1][k];

  /*
   * now the not enclosed by any base pair terms for whatever it is we do not need anymore...
   * ... which should be e.g; k, again
   */
  for (startu = MIN2(ulength, k); startu > 0; startu--) {
    temp = 0.;
    for (i5 = MAX2(1, k - winSize + 2); i5 <= MIN2(k - startu, n - winSize + 1); i5++)
      temp += q[i5][k - startu] * q[k + 1][i5 + winSize - 1] * scale[startu] / q[i5][i5 + winSize - 1];
    /* the 2 Cases where the borders are on the edge of the interval */
    if ((k >= winSize) && (startu + 1 <= winSize))
      temp += q[k - winSize + 1][k - startu] * scale[startu] / q[k - winSize + 1][k];

    if ((k <= n - winSize + startu) && (k - startu >= 0) && (k < n) && (startu + 1 <= winSize))
      temp += q[k + 1][k - startu + winSize] * scale[startu] / q[k - startu + 1][k - startu + winSize];

    /* Divide by number of possible windows */
    pU[k][startu]   += temp;
    pUO[k][startu]  += temp;

    {
      int leftmost, rightmost;

      leftmost      = MAX2(1, k - winSize + 1);
      rightmost     = MIN2(n - winSize + 1, k - startu + 1);
      pU[k][startu] /= (rightmost - leftmost + 1);
      /* Do we want to make a distinction between those? */
      pUH[k][startu]  /= (rightmost - leftmost + 1);
      pUO[k][startu]  /= (rightmost - leftmost + 1);
      pUI[k][startu]  /= (rightmost - leftmost + 1);
      pUM[k][startu]  /= (rightmost - leftmost + 1);
    }
  }
  free(QBE);
  free(QBI);
  free(QBH);
  free(QBM);
  return;
}


PUBLIC void
putoutpU_prob_splitup(double  **pU,
                      double  **pUO,
                      double  **pUH,
                      double  **pUI,
                      double  **pUM,
                      int     length,
                      int     ulength,
                      FILE    *fp,
                      int     energies)
{
  /* put out unpaireds */
  int     i, k;
  double  kT = (temperature + K0) * GASCONST / 1000.0;
  double  temp;

  if (energies)
    fprintf(fp, "#opening energies\n #i$\tl=");
  else
    fprintf(fp, "#unpaired probabilities\n #i$\tl=");

  fprintf(fp, "Total\n");
  for (i = 1; i <= ulength; i++)
    fprintf(fp, "%d\t", i);
  fprintf(fp, "\n");

  for (k = 1; k <= length; k++) {
    fprintf(fp, "%d\t", k);
    for (i = 1; i <= ulength; i++) {
      if (i > k) {
        fprintf(fp, "NA\t");
        continue;
      }

      if (energies)
        temp = -log(pU[k][i]) * kT;
      else
        temp = pU[k][i];

      fprintf(fp, "%.7g\t", temp);
    }
    fprintf(fp, "\tT\n");
    free(pU[k]);
  }
  fprintf(fp, "\n###################################################################\nHairpin\n");
  for (i = 1; i <= ulength; i++)
    fprintf(fp, "%d\t", i);
  fprintf(fp, "\n");

  for (k = 1; k <= length; k++) {
    fprintf(fp, "%d\t", k);
    for (i = 1; i <= ulength; i++) {
      if (i > k) {
        fprintf(fp, "NA\t");
        continue;
      }

      if (energies)
        temp = -log(pUH[k][i]) * kT;
      else
        temp = pUH[k][i];

      fprintf(fp, "%.7g\t", temp);
    }
    fprintf(fp, "\tH\n");
    free(pUH[k]);
  }
  fprintf(fp, "\n###################################################################\nInterior\n");
  for (i = 1; i <= ulength; i++)
    fprintf(fp, "%d\t", i);
  fprintf(fp, "\n");

  for (k = 1; k <= length; k++) {
    fprintf(fp, "%d\t", k);
    for (i = 1; i <= ulength; i++) {
      if (i > k) {
        fprintf(fp, "NA\t");
        continue;
      }

      if (energies)
        temp = -log(pUI[k][i]) * kT;
      else
        temp = pUI[k][i];

      fprintf(fp, "%.7g\t", temp);
    }
    fprintf(fp, "\tI\n");
    free(pUI[k]);
  }
  fprintf(fp, "\n###################################################################\nMultiloop\n");
  for (i = 1; i <= ulength; i++)
    fprintf(fp, "%d\t", i);
  fprintf(fp, "\n");

  for (k = 1; k <= length; k++) {
    fprintf(fp, "%d\t", k);
    for (i = 1; i <= ulength; i++) {
      if (i > k) {
        fprintf(fp, "NA\t");
        continue;
      }

      if (energies)
        temp = -log(pUM[k][i]) * kT;
      else
        temp = pUM[k][i];

      fprintf(fp, "%.7g\t", temp);
    }
    fprintf(fp, "\tM\n");
    free(pUM[k]);
  }
  fprintf(fp, "\n###################################################################\nExterior\n");
  for (i = 1; i <= ulength; i++)
    fprintf(fp, "%d\t", i);
  fprintf(fp, "\t E\n");

  for (k = 1; k <= length; k++) {
    fprintf(fp, "%d\t", k);
    for (i = 1; i <= ulength; i++) {
      if (i > k) {
        fprintf(fp, "NA\t");
        continue;
      }

      if (energies)
        temp = -log(pUO[k][i]) * kT;
      else
        temp = pUO[k][i];

      fprintf(fp, "%.7g\t", temp);
    }
    fprintf(fp, "\n");
    free(pU[k]);
  }
  fflush(fp);
}


/*###########################################*/
/*# deprecated functions below              #*/
/*###########################################*/

PRIVATE plist *
wrap_pf_foldLP(char             *sequence,
             int              winSize,
             int              pairSize,
             float            cutoffb,
             double           **pU,
             plist            **dpp2,
             FILE             *pUfp,
             FILE             *spup,
             vrna_exp_param_t *parameters)
{

  vrna_fold_compound_t  *vc;
  vrna_md_t             md;

  vc = NULL;

  /* we need vrna_exp_param_t datastructure to correctly init default hard constraints */
  if(parameters)
    md = parameters->model_details;
  else{
    set_model_details(&md); /* get global default parameters */
  }
  md.compute_bpp  = 1;        /* turn on base pair probability computations */
  md.window_size  = winSize;  /* set size of sliding window */
  md.max_bp_span  = pairSize; /* set maximum base pair span */

  vc = vrna_fold_compound(sequence, &md, VRNA_OPTION_PF | VRNA_OPTION_WINDOW);

  /* prepare exp_params and set global pf_scale */
  free(vc->exp_params);
  vc->exp_params = vrna_exp_params(&md);
  vc->exp_params->pf_scale = pf_scale;

  if(backward_compat_compound && backward_compat)
    vrna_fold_compound_free(backward_compat_compound);

  backward_compat_compound  = vc;
  backward_compat           = 1;
  iindx = backward_compat_compound->iindx; /* for backward compatibility and Perl wrapper */

  return vrna_pf_window(vc, pU, dpp2, pUfp, spup, cutoffb);
}


PUBLIC void
init_pf_foldLP(int length)
{
  /* DO NOTHING */
}


PUBLIC void
update_pf_paramsLP(int length){

  if(backward_compat_compound && backward_compat){
    vrna_md_t         md;
    set_model_details(&md);
    vrna_exp_params_reset(backward_compat_compound, &md);

    /* compatibility with RNAup, may be removed sometime */
    pf_scale = backward_compat_compound->exp_params->pf_scale;
  }
}


PUBLIC void
update_pf_paramsLP_par( int length,
                      vrna_exp_param_t *parameters){

  if(backward_compat_compound && backward_compat){
    vrna_md_t         md;
    if(parameters){
      vrna_exp_params_subst(backward_compat_compound, parameters);
    } else {
      set_model_details(&md);
      vrna_exp_params_reset(backward_compat_compound, &md);
    }

    /* compatibility with RNAup, may be removed sometime */
    pf_scale = backward_compat_compound->exp_params->pf_scale;
  }
}


PUBLIC plist *
pfl_fold(char   *sequence,
         int    winSize,
         int    pairSize,
         float  cutoffb,
         double **pU,
         plist  **dpp2,
         FILE   *pUfp,
         FILE   *spup)
{
  return wrap_pf_foldLP(sequence, winSize, pairSize, cutoffb, pU, dpp2, pUfp, spup, NULL);
}

PUBLIC plist *
pfl_fold_par(char             *sequence,
             int              winSize,
             int              pairSize,
             float            cutoffb,
             double           **pU,
             plist            **dpp2,
             FILE             *pUfp,
             FILE             *spup,
             vrna_exp_param_t *parameters)
{
  return wrap_pf_foldLP(sequence, winSize, pairSize, cutoffb, pU, dpp2, pUfp, spup, parameters);
}

PUBLIC void
putoutpU_prob(double  **pU,
              int     length,
              int     ulength,
              FILE    *fp,
              int     energies)
{
  if(backward_compat_compound && backward_compat){
    putoutpU_prob_par(pU, length, ulength, fp, energies, backward_compat_compound->exp_params);
  } else {
    vrna_message_warning("putoutpU_prob: Not doing anything! First, run pfl_fold()!");
  }
}


PUBLIC void
putoutpU_prob_par(double            **pU,
                  int               length,
                  int               ulength,
                  FILE              *fp,
                  int               energies,
                  vrna_exp_param_t  *parameters)
{
  /* put out unpaireds */
  int     i, k;
  double  kT = parameters->kT / 1000.0;
  double  temp;

  if (energies)
    fprintf(fp, "#opening energies\n #i$\tl=");
  else
    fprintf(fp, "#unpaired probabilities\n #i$\tl=");

  for (i = 1; i <= ulength; i++)
    fprintf(fp, "%d\t", i);
  fprintf(fp, "\n");

  for (k = 1; k <= length; k++) {
    fprintf(fp, "%d\t", k);
    for (i = 1; i <= ulength; i++) {
      if (i > k) {
        fprintf(fp, "NA\t");
        continue;
      }

      if (energies)
        temp = -log(pU[k][i]) * kT;
      else
        temp = pU[k][i];

      fprintf(fp, "%.7g\t", temp);
    }
    fprintf(fp, "\n");
    free(pU[k]);
  }
  fflush(fp);
}


PUBLIC void
putoutpU_prob_bin(double  **pU,
                  int     length,
                  int     ulength,
                  FILE    *fp,
                  int     energies)
{
  if(backward_compat_compound && backward_compat){
    putoutpU_prob_bin_par(pU, length, ulength, fp, energies, backward_compat_compound->exp_params);
  } else {
    vrna_message_warning("putoutpU_prob_bin: Not doing anything! First, run pfl_fold()!");
  }
}


PUBLIC void
putoutpU_prob_bin_par(double            **pU,
                      int               length,
                      int               ulength,
                      FILE              *fp,
                      int               energies,
                      vrna_exp_param_t  *parameters)
{
  /* put out unpaireds */
  int     i, k;
  double  kT = parameters->kT / 1000.0;
  int     *p;

  p = (int *)vrna_alloc(sizeof(int) * 1);
  /* write first line */
  p[0] = ulength; /* u length */
  fwrite(p, sizeof(int), 1, fp);
  p[0] = length;  /* seq length */
  fwrite(p, sizeof(int), 1, fp);
  for (k = 3; k <= (length + 20); k++) {
    /* all the other lines are set to 1000000 because we are at ulength=0 */
    p[0] = 1000000;
    fwrite(p, sizeof(int), 1, fp);
  }
  /* data */
  for (i = 1; i <= ulength; i++) {
    for (k = 1; k <= 11; k++) {
      /* write first ten entries to 1000000 */
      p[0] = 1000000;
      fwrite(p, sizeof(int), 1, fp);
    }
    for (k = 1; k <= length; k++) {
      /* write data now */
      if (i > k) {
        p[0] = 1000000;         /* check if u > pos */
        fwrite(p, sizeof(int), 1, fp);
        continue;
      } else {
        p[0] = (int)rint(100 * (-log(pU[k][i]) * kT));
        fwrite(p, sizeof(int), 1, fp);
      }
    }
    for (k = 1; k <= 9; k++) {
      /* finish by writing the last 10 entries */
      p[0] = 1000000;
      fwrite(p, sizeof(int), 1, fp);
    }
  }
  /* free pU array; */
  for (k = 1; k <= length; k++)
    free(pU[k]);
  free(p);
  fflush(fp);
}



