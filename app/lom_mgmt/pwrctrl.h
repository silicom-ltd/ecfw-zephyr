#ifndef _LOM_MGMT_PWRCTRL_H_
#define _LOM_MGMT_PWRCTRL_H_

#ifdef CONFIG_LOM_MGMT_FUNC_POWER_CTRL

int lom_pwrctrl_thread_start(void);
int lom_pwrctrl_request(uint8_t act, uint8_t* params, uint8_t param_cnt);
int lom_pwrctrl_result(int* ret);

/*
 * Need to be called when SLP_WLAN=0, PLTRST=0, SUS_PWRDN_ACK=1
 */
void lom_pwrctrl_on_signal(uint32_t signal, uint32_t value);

#endif

#endif /* _LOM_MGMT_PWRCTRL_H_ end  */
