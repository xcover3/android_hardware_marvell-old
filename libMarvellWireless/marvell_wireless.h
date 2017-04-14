#ifndef __WIRELESS_INTERFACE_
#define __WIRELESS_INTERFACE_
int wifi_enable(void);
int wifi_disable(void);
int wifi_set_drv_arg(const char * wifi_drv_arg);
int uap_enable(void);
int uap_disable(void);
int bluetooth_enable(void);
int bluetooth_disable(void);
int bluetooth_poweron(void);
int bluetooth_poweroff(void);
int bt_set_drv_arg(const char * bt_drv_arg);
int fm_enable(void);
int fm_disable(void);

/* Force marvell wireless chip power off
  * Return value:
  * 0 : The command has been implemented;
  * 1 : The command is not supported(The feadture is disabled).
  *----------------------------------------------------
  * To enable/disable the feature, please set the property:
  * persist.sys.mrvl_wl_recovery
  * 1: enable; 0: disable.
  */
int wifi_uap_force_poweroff();
int bt_fm_force_poweroff();
int wifi_get_fwstate();
#endif

