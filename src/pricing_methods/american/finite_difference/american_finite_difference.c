#include <string.h>
#include <stdlib.h>
#include <result_internal.h>
#include <option_data.h>
#include <option_data_internal.h>
#include <pricing_method_internal.h>
#include <qss-solver/black_scholes_model.h>
#include <impl_vol_methods/impl_vol_methods.h>
#include "american_finite_difference.h"

static double lagrange_interpolation(double X, double *x, double *y, int N) {
  double result = 0, temp;
  int i, j;

  for (i = 0; i < N; i++) {
    temp = 1;
    for (j = 0; j < N; j++) {
      if (j != i)
        temp *= (X - x[j]) / (x[i] - x[j]);
    }
    result += temp * y[i];
  }

  return result;
}

static BSM_OT option_type_BSM_OT(option_type ot) {
  switch (ot) {
    case OPTION_CALL:
      return CALL;
    case OPTION_PUT:
      return PUT;
  }
  return CALL;
}

static BSM BSM_(int grid_size, double Smax, double tol, double abstol,
  option_data od, pricing_data pd) {

  option_type type = od->opt_type;
  BSM_OT ot = option_type_BSM_OT(type);
  time_period tp = od->maturity;
  date maturity = tp_get_date(tp);
  double period = tp_get_period(tp);

  volatility  sigma = pd->vol;
  risk_free_rate r = pd->r;
  dividend divi = pd->d;
  double K = od->strike;

  double d;
  int dd_n;
  double *dd_d, *dd_a;
  if (div_get_type(divi) == DIV_CONTINUOUS) {
    d = div_cont_get_val(divi);
    dd_n = 0;
    dd_d = NULL;
    dd_a = NULL;
  } else {
    d = 0.0;
    dd_n = div_disc_get_n(divi);
    dd_d = div_disc_get_dates(divi);
    dd_a = div_disc_get_ammounts(divi);
  }

  return new_BSM(grid_size, ot, Smax, sigma, r, K, d, dd_n, dd_d, dd_a, period,
    maturity, tol, abstol);
}

static void apply_div(dividend d, double *S) {
  if (div_get_type(d) == DIV_DISCRETE) {
    int ndivs = div_disc_get_n(d);
    double *ammounts = div_disc_get_ammounts(d);

    int size = sizeof(S) / sizeof(double);
    int i, j;
    for (i = 0; i < size; i++)
      for (j = 0; j < ndivs; j++)
        S[i] -= ammounts[j];
  }
}

static double query_value(BSM *bsm, BSM_F bsmf, double S, int N, double Smax) {
  int i;
  static const int np = 4;
  double y50[np], y100[np], s[np], y[np];

  double ds = Smax / ((double) N);
  int n = (int) ((double) N / Smax * S);
  int p50[4] = { n-1, n, n+1, n+2 };
  int p100[4] = { 2*n-2, 2*n, 2*n+2, 2*n+4 };

  for (i = 0; i < np; i++) {
    s[i] = ds * p50[i];
    y50[i] = bsmf(bsm[0], p50[i]);
    y100[i] = bsmf(bsm[1], p100[i]);
    /* richardson */
    y[i] = (4.0 * y100[i] - y50[i]) / 3.0;
  }

  return lagrange_interpolation(S, s, y, np);
}

static void calculate_bsmf(BSM_F bsmf, option_data od, pricing_data pd,
  int size, double *Ss, pm_settings pms, double *output) {

  int N = pm_settings_get_grid_size(pms);
  double tol = pm_settings_get_tol(pms);
  double abstol = pm_settings_get_abstol(pms);

  int i;
  double K = od->strike,
         Smax = pm_settings_get_Smax(pms);

  if (Smax < 0)
    Smax = 5 * K;

  BSM bsm[2];

  //#pragma omp parallel for
  for (i = 0; i < 2; i++) {
    bsm[i] = BSM_(N + (N * i), Smax, tol, abstol, od, pd);
  }

  double *Ss_ = (double *) malloc(sizeof(double) * size);
  memcpy(Ss_, Ss, sizeof(double) * size);

  apply_div(pd->d, Ss_);

  for(i = 0; i < size; i++)
    output[i] = query_value(bsm, bsmf, Ss_[i], N, Smax);

  free(Ss_);
  delete_BSM(bsm[0]);
  delete_BSM(bsm[1]);
}

static double iv_f(volatility vol, impl_vol_mf_args ivmfa) {
  pricing_data pd = ivmfa->pd;
  pd->vol = vol;
  double price;
  calculate_bsmf(BSM_v, ivmfa->od, pd, 1, &ivmfa->S, ivmfa->pms, &price);
  return price - ivmfa->V;
}

static int impl_vol(option_data od, pricing_data pd, double V, double S,
  result ret, pm_settings pms, void *pm_data) {

  impl_vol_mf_args ivmfa = new_impl_vol_mf_args(od, pd, V, S, pms, pm_data);

  int res = secant_method(iv_f, ivmfa, ret);

  delete_impl_vol_mf_args(ivmfa);

  return res;
}

static int option_price(option_data od, pricing_data pd, double S,
  result ret, pm_settings pms, void *pm_data) {

  if (od->exercise != AM_EXERCISE)
    return -1;

  double price;
  calculate_bsmf(BSM_v, od, pd, 1, &S, pms, &price);

  return result_set_price(ret, price);
}

static int option_prices(option_data od, pricing_data pd, int size, double *Ss,
  result ret, pm_settings pms, void *pm_data) {

  if (od->exercise != AM_EXERCISE)
    return -1;

  if (!Ss)
    return 0;

  double *prices = (double *) malloc(sizeof(double) * size);

  calculate_bsmf(BSM_v, od, pd, size, Ss, pms, prices);

  return result_set_prices(ret, prices);
}

static int greek_delta(option_data od, pricing_data pd, double S,
  result ret, pm_settings pms, void *pm_data) {

  if (od->exercise != AM_EXERCISE)
    return -1;

  double delta;
  calculate_bsmf(BSM_delta, od, pd, 1, &S, pms, &delta);

  return result_set_delta(ret, delta);
}

static int greek_gamma(option_data od, pricing_data pd, double S,
  result ret, pm_settings pms, void *pm_data) {

  if (od->exercise != AM_EXERCISE)
    return -1;

  double gamma;
  calculate_bsmf(BSM_gamma, od, pd, 1, &S, pms, &gamma);

  return result_set_gamma(ret, gamma);
}

static int greek_theta(option_data od, pricing_data pd, double S,
  result ret, pm_settings pms, void *pm_data) {

  if (od->exercise != AM_EXERCISE)
    return -1;

  double theta;
  calculate_bsmf(BSM_theta, od, pd, 1, &S, pms, &theta);

  return result_set_theta(ret, theta);
}

static int greek_rho(option_data od, pricing_data pd, double S,
  result ret, pm_settings pms, void *pm_data) {

  if (od->exercise != AM_EXERCISE)
    return -1;

  static const double delta = 0.0001;

  pricing_data pd0 = new_pricing_data_(pd);

  risk_free_rate r = pd0->r;

  pd0->r = r - delta;
  double f1;
  calculate_bsmf(BSM_v, od, pd0, 1, &S, pms, &f1);

  pd0->r = r + delta;
  double f2;
  calculate_bsmf(BSM_v, od, pd0, 1, &S, pms, &f2);

  double rho =  (f2 - f1) / (2 * delta);

  delete_pricing_data(pd0);
  return result_set_rho(ret, rho);
}

static int greek_vega(option_data od, pricing_data pd, double S,
  result ret, pm_settings pms, void *pm_data) {

  if (od->exercise != AM_EXERCISE)
    return -1;

  static const double delta = 0.0001;

  pricing_data pd0 = new_pricing_data_(pd);

  volatility vol = pd0->vol;

  pd0->vol = vol - delta;
  double f1;
  calculate_bsmf(BSM_v, od, pd0, 1, &S, pms, &f1);

  pd0->vol = vol + delta;
  double f2;
  calculate_bsmf(BSM_v, od, pd0, 1, &S, pms, &f2);

  double vega =  (f2 - f1) / (2 * delta);

  delete_pricing_data(pd0);
  return result_set_vega(ret, vega);
}

pricing_method new_american_finite_difference(pricing_data pd) {
  return new_pricing_method_(option_price, option_prices, greek_delta,
    greek_gamma, greek_theta, greek_rho, greek_vega, impl_vol, NULL, pd, NULL);
}
