#ifndef __IMPL_VOL_MF_H__
#define __IMPL_VOL_MF_H__

#include <common.h>

typedef struct impl_vol_mf_args_ * impl_vol_mf_args;
/* better name ?? */
struct impl_vol_mf_args_ {
  option_data od;
  pricing_data pd;
  double V;
  double S;
  pm_settings pms;
  void* pm_data;
};

typedef double (*impl_vol_mf) (double vol, impl_vol_mf_args);

impl_vol_mf_args new_impl_vol_mf_args(option_data od, pricing_data pd, double V,
  double S, pm_settings pms, void *pm_data);

void delete_impl_vol_mf_args(impl_vol_mf_args);

#endif /* __IMPL_VOL_MF_H__ */
