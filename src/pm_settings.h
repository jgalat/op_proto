#ifndef __PM_SETTINGS_H__
#define __PM_SETTINGS_H__

typedef struct pm_settings_ * pm_settings;
struct pm_settings_;

pm_settings new_pm_settings();
void delete_pm_settings(pm_settings);

int pm_settings_set_N(pm_settings, int);
int pm_settings_set_tol(pm_settings, double);
int pm_settings_set_abstol(pm_settings, double);
int pm_settings_set_iv_max_it(pm_settings, int);
int pm_settings_set_iv_eps(pm_settings, double);
int pm_settings_set_iv_init(pm_settings, double, double);

int pm_settings_get_N(pm_settings);
double pm_settings_get_tol(pm_settings);
double pm_settings_get_abstol(pm_settings);
int pm_settings_get_iv_max_it(pm_settings);
double pm_settings_get_iv_eps(pm_settings);
double *pm_settings_get_iv_init(pm_settings);

#endif /* __PM_SETTINGS_H__ */