#ifdef CONFIG_LOM_MGMT_FUNC_POWER_CTRL
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/espi.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

#include "board_config.h"
#include "gpio_ec.h"
#include "pwrplane.h"
#include "task_handler.h"

#include "lom_mgmt_proc_inc.h"

#include "pwrctrl.h"

LOG_MODULE_DECLARE(lom_mgmt, CONFIG_LOM_MGMT_PROC_LOG_LEVEL);

static const struct device *const espi_dev = DEVICE_DT_GET(DT_NODELABEL(espi0));

K_SEM_DEFINE(lom_pwrctrl_request_sem, 0, 1);
K_SEM_DEFINE(lom_pwrctrl_started_sem, 0, 1);

static void lom_pwrctrl_thread(void *p1, void *p2, void *p3);

K_THREAD_DEFINE(lom_pwrctrl_tid, 1024, lom_pwrctrl_thread,
	NULL, NULL, NULL, EC_TASK_PRIORITY, K_INHERIT_PERMS, -1);

#define CONSUME_RESUME_SIGNALS_WAIT_TIME CONFIG_HOST_FORCE_DOWN_WAIT_TIME

#define WAIT_SIG_SLEEP_TIME_MS 10
#define MS_TIMEOUT_TO_CNT(t)						\
	(((t) + WAIT_SIG_SLEEP_TIME_MS - 1) / WAIT_SIG_SLEEP_TIME_MS)

typedef bool (*wait_cond_t)(int val, int exp_val);

static atomic_t lom_pwrctrl_busy;

struct pwrctrl_work_data
{
        uint8_t act;

	uint8_t params[2]; /* Optional params for FORCE_DOWN:
			    *  params[0]: enable gracefull shutdown
			    *  params[1]; the customized timeout (unit is 10s). 0: use default
			    */

	uint8_t processed;

	int     ret;
};
static struct pwrctrl_work_data pwrctrl_cmd;

static volatile int in_force_down;

/*
 * be used in the 1st step of pwrctrl_do_force_down()(gracefull shutdown), for checking
 * if the shutdown process be triggered
 *
 * be used in the 2nd step of pwrctrl_do_force_down(), for checking if a normal shutdown
 * process occurring in force_down step. (Ref, the comment in lom_pwrctrl_on_signal())
 */
static volatile int sig_pltrst_asserted;


/*
 * be used in the 2nd step of pwrctrl_do_force_down(), record the signal wakup us
 */
#define WAKEUP_SIG_NOSIG -1
#define WAKEUP_SIG_UNSET -2

static volatile int force_down_wakeup_signal = WAKEUP_SIG_UNSET;

#ifdef CONFIG_ATTEMPT_CONSUME_RESUME_SIGNALS
/*
 * be used in the 3rd step of force_down process, for checking if the Resume process
 * has started.
 */
static volatile int sig_sus_pwrdn_ack_deasserted;
#endif


static inline bool force_down_post(int sig)
{
	if (force_down_wakeup_signal == WAKEUP_SIG_NOSIG) {
		gpio_write_pin(PM_PWRBTN, 1);
		force_down_wakeup_signal = sig;
		return true;
	}

	return false;
}

static int pwrctrl_do_up(void)
{
	LOG_DBG_PWRCTRL(">> Do UP");

	gpio_write_pin(PM_PWRBTN, 0);
	k_msleep(150);
	gpio_write_pin(PM_PWRBTN, 1);

	LOG_DBG_PWRCTRL(">> Do UP OK");

	return 0;
}

static int pwrctrl_do_shutdown(void)
{
	LOG_DBG_PWRCTRL(">> Do Normal Down");

	gpio_write_pin(PM_PWRBTN, 0);
	k_msleep(150);
	gpio_write_pin(PM_PWRBTN, 1);

	LOG_DBG_PWRCTRL(">> Do Normal Down OK");

	return 0;
}

static inline bool is_not_less(int val, int exp_val)
{
	return (val >= exp_val);
}

static inline bool is_equal(int val, int exp_val)
{
	return (val == exp_val);
}

static inline bool is_vware_sig_equal(int sig, int exp_val)
{
	uint8_t level;

	if (espi_receive_vwire(espi_dev, sig, &level) != 0) {
		return false;
	}

	return (level == exp_val);
}

static int wait_sig_value(volatile int *sig, wait_cond_t expr, int exp_val, uint32_t timeout)
{
	uint32_t loop_cnt = MS_TIMEOUT_TO_CNT(timeout);

	while (loop_cnt) {
		if (expr(*sig, exp_val)) {
			return 0;
		}
		k_msleep(WAIT_SIG_SLEEP_TIME_MS);
		loop_cnt--;
	}

	return -ETIMEDOUT;
}

static int wait_sig_value_with_init(volatile int *sig, int set_val, wait_cond_t expr, int exp_val,
	uint32_t timeout)
{
	*sig = set_val;

	return wait_sig_value(sig, expr, exp_val, timeout);
}

static int wait_vware_sig_value(enum espi_vwire_signal signal, int exp_val, uint32_t timeout)
{
	int sig_checked = signal;

	return wait_sig_value(&sig_checked, is_vware_sig_equal, exp_val, timeout);
}

static int wait_host_status(uint8_t exp_val, uint32_t timeout)
{
	uint32_t loop_cnt = MS_TIMEOUT_TO_CNT(timeout);

	while (loop_cnt) {
		if (pwrseq_system_state() == exp_val) {
			return 0;
		}

		k_msleep(WAIT_SIG_SLEEP_TIME_MS);
		loop_cnt--;
	}

	return -ETIMEDOUT;
}

static int pwrctrl_do_force_down(void)
{
	int ret = 0;
	int did_gracefull_shutdown = 0;

	LOG_DBG_PWRCTRL(">> Do Force Down");

	in_force_down = 1;

	/** 1st step: try gracefull shutdown **/

#ifdef CONFIG_ATTEMPT_GRACEFUL_SHUTDOWN
	/*
	 * When CONFIG_ATTEMPT_GRACEFUL_SHUTDOWN=y, user can control if do gracefull shutdown
	 */
	if (pwrctrl_cmd.params[0]) { /* enabled gracefull shutdown */
		uint32_t timeout_ms;

		if (pwrctrl_cmd.params[1]) { /* user specified the timeout */
			timeout_ms = pwrctrl_cmd.params[1] * 10000;
		}
		else {
			timeout_ms = CONFIG_HOST_GRACEFUL_SHUTDOWN_WAIT_TIME;
		}

		LOG_DBG_PWRCTRL("gracefull shutdown timeout %d(s)", timeout_ms/1000);

		did_gracefull_shutdown = 1;

		sig_pltrst_asserted = 0;

		pwrctrl_do_shutdown();

		ret = wait_sig_value(&sig_pltrst_asserted, is_equal, 1, timeout_ms);
		if (ret == 0) {
			LOG_DBG_PWRCTRL(">> Do Force Down OK");
			goto exit;
		}

		LOG_DBG_PWRCTRL(">> Gracefull Shutdown failed, try Force Down");
	}
#endif

	/** 2nd step: try force shutdown **/

#ifdef CONFIG_ATTEMPT_CONSUME_RESUME_SIGNALS
	/*
	 * ensure SUS_PWRDN_ACK is asserted. this check is done purely to ensure
	 * logical consistency
	 */
	if (wait_vware_sig_value(ESPI_VWIRE_SIGNAL_SUS_PWRDN_ACK, 0, 2000) == 0) {
		sig_sus_pwrdn_ack_deasserted = 0;
	}
	else {
		LOG_ERR("SUS_PWRDN_ACK remains deasserted");
		goto exit;
	}
#endif

	gpio_write_pin(PM_PWRBTN, 0);

	/*
	 * This step will trigger a Normal shutdown(1st step is disabled) or suspend-to-S4
	 *
	 * NOTE:
	 *   Whether this step triggers a Normal Shutdown or Suspend-to-S4
	 *   when the 1st step is disabled IS NOT FIXIED (tested on Ibiza),
	 *   potentially linked to the system running on the host.
	 */
	ret = wait_sig_value_with_init(&force_down_wakeup_signal, WAKEUP_SIG_NOSIG, is_not_less, 0,
		CONFIG_HOST_FORCE_DOWN_WAIT_TIME);
	if (ret) {
		LOG_ERR(">> Do Force Down Wait Timeout");
		force_down_post(WAKEUP_SIG_UNSET);
		goto exit;
	}

	/** 3rd step: try cosume signals of Resume **/

	/*
	 * Last wait_sig be awoke by signal PLTRST or SLP_WLAN
	 *   Normal Shutdown : PLTRST first
	 *   Suspend-to-S4   : SLP_WLAN, no PLTRST
	 */
	if (force_down_wakeup_signal == ESPI_VWIRE_SIGNAL_PLTRST) {
		LOG_INF("(**) Normal Shutdown is triggered");

		if (did_gracefull_shutdown) {
			LOG_WRN("An unexpected shutdown process was triggered");
		}
	}
#ifdef CONFIG_ATTEMPT_CONSUME_RESUME_SIGNALS
	else if (force_down_wakeup_signal == ESPI_VWIRE_SIGNAL_SLP_WLAN) {
		LOG_INF("(**) Suspend-to-S4 is triggered");

		LOG_DBG_PWRCTRL("Consume signals of Resume...");
		ret = wait_sig_value(&sig_sus_pwrdn_ack_deasserted, is_equal, 1,
			CONSUME_RESUME_SIGNALS_WAIT_TIME);
		if (ret) {
			LOG_ERR(">> Consume signals of Resume timeout");
			goto exit;
		}
		LOG_DBG_PWRCTRL("Consume signals of Resume OK");
	}
	else {
		LOG_ERR("Awoke By Unexpected Signal");
	}
#endif

	LOG_DBG_PWRCTRL(">> Do Force Down OK");

 exit:
	in_force_down = 0;

	return ret;
}

static int pwrctrl_do_hard_reset(void)
{
	LOG_DBG_PWRCTRL(">> Do Hard Reset");

	gpio_write_pin(SOC_RSTBTN_N, 0);
	k_msleep(20);
	gpio_write_pin(SOC_RSTBTN_N, 1);

	LOG_DBG_PWRCTRL(">> Do Hard Reset OK");

	return 0;
}

static int pwrctrl_do_power_cycle(void)
{
	int ret;

	LOG_DBG_PWRCTRL(">> Do Power Cycle");

	if ((ret = pwrctrl_do_force_down()) < 0) {
		return ret;
	}

	/*
	 * The timeout param is approximate; exact precision is not guaranteed or required:
	 *   the critical sequencing guarantees should be implemented in pwrctrl_do_force_down().
	 */
	if ((ret = wait_host_status(SYSTEM_S5_STATE, 10000)) < 0) {
		LOG_ERR("Timed out waiting for host to go down");
		return ret;
	}

	LOG_DBG_PWRCTRL("Host is already down");

#if defined(CONFIG_BOARD_MEC172X_ADL_N_CP)
	switch_card_power_control(0);
#endif

	/* Enable time response for Power LED */
	k_msleep(2000);

#if defined(CONFIG_BOARD_MEC172X_ADL_N_CP)
	switch_card_power_control(1);
#endif

	if ((ret = pwrctrl_do_up()) < 0) {
		return ret;
	}

	/*
	 * The timeout param is approximate; exact precision is not guaranteed or required:
	 *   the pwrctrl_do_up() always quickly and reliably initiates the power-up process,
	 *   unless a hardware failure occurs.
	 */
	if ((ret = wait_host_status(SYSTEM_S0_STATE, 5000)) < 0) {
		LOG_ERR("Timed out waiting for host to come up");
		return ret;
	}

	LOG_DBG_PWRCTRL("Host is already up");

	LOG_DBG_PWRCTRL(">> Do Power Cycle End");

	return 0;
}

int (*pwrctrl_funcs[])(void) = {
	NULL,
	pwrctrl_do_up,
	pwrctrl_do_shutdown,
	pwrctrl_do_hard_reset,
	pwrctrl_do_force_down,
	pwrctrl_do_power_cycle,
};

static void lom_pwrctrl_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	k_sem_give(&lom_pwrctrl_started_sem);

	while (true) {
		k_sem_take(&lom_pwrctrl_request_sem, K_FOREVER);

		atomic_set(&lom_pwrctrl_busy, 1);
		pwrctrl_cmd.ret = pwrctrl_funcs[pwrctrl_cmd.act]();
		pwrctrl_cmd.processed = 1;
		atomic_set(&lom_pwrctrl_busy, 0);
	}
}

int lom_pwrctrl_request(uint8_t act, uint8_t* params, uint8_t param_cnt)
{
	if (param_cnt > sizeof(pwrctrl_cmd.params)) {
		return -ENOSPC;
	}

	if (atomic_get(&lom_pwrctrl_busy)) {
		return -EBUSY;
	}

	/*
	 * The PwrCtrl thread is still pending scheduling and cannot accept new requests yet
	 */
	if (k_sem_count_get(&lom_pwrctrl_request_sem)) {
		return -EAGAIN;
	}

	pwrctrl_cmd.act = act;
	pwrctrl_cmd.processed = 0;
	pwrctrl_cmd.ret = 0;

	memset(pwrctrl_cmd.params, 0, sizeof(pwrctrl_cmd.params));
	if (param_cnt) {
		memcpy(pwrctrl_cmd.params, params, param_cnt);
	}

	LOG_DBG_PWRCTRL("PwrCtrl Req: act=%d, pcnt=%d, p[0]=0x%02x, p[1]=0x%02x", act,
		param_cnt, pwrctrl_cmd.params[0], pwrctrl_cmd.params[1]);

	k_sem_give(&lom_pwrctrl_request_sem);

	return 0;
}

static inline bool check_sig_asserted(uint32_t signal, uint32_t value, enum espi_vwire_signal exp_sig)
{
	return ((signal == exp_sig) && (value == 0));
}

static inline bool check_sig_deasserted(uint32_t signal, uint32_t value, enum espi_vwire_signal exp_sig)
{
	return ((signal == exp_sig) && (value == 1));
}

/*
 * only take effect in process of pwrctrl_do_force_down()
 *
 *  focus on signals:
 *    ESPI_VWIRE_SIGNAL_SLP_WLAN      = 0: start a Suspend-to-S4 sequence,
 *    ESPI_VWIRE_SIGNAL_PLTRST        = 0: start a Normal Shutdown sequence
 *    ESPI_VWIRE_SIGNAL_SUS_PWRDN_ACK = 1: end of Resume sequence
 */
void lom_pwrctrl_on_signal(uint32_t signal, uint32_t value)
{
	if (!in_force_down) {
		return;
	}

	/* the 2nd step of powerctrl_do_force_down() focuses on this signal. */
	if (check_sig_asserted(signal, value, ESPI_VWIRE_SIGNAL_SLP_WLAN)) {
		force_down_post(ESPI_VWIRE_SIGNAL_SLP_WLAN);
	}
	/* the step 1/2 of powerctrl_do_force_down() focuses on this signal */
	else if (check_sig_asserted(signal, value, ESPI_VWIRE_SIGNAL_PLTRST)) {
		/*
		 * 1. (Gracefull Shutdown is enable)
		 *   host take a long time (more than HOST_GRACEFUL_SHUTDOWN_WAIT_TIME) to start a normal
		 *   shutdown process, causing the process enter to forced shutdown step.
		 *   On this case, the gpio PM_PWRBTN needs to be deasserted to prevent
		 *   the host from restarting.
		 *
		 * 2. (Gracefull Shutdown is disable)
		 *   the 2nd step of powerctrl_do_force_down() CHANGE to trigger a Normal Shutdown
		 */
		if (force_down_post(ESPI_VWIRE_SIGNAL_PLTRST)) {
			return;
		}
#ifdef CONFIG_ATTEMPT_GRACEFUL_SHUTDOWN
		/* the 1st step of powerctrl_do_force_down() focuses on this signal. */
		sig_pltrst_asserted = 1;
#endif
	}
#ifdef CONFIG_ATTEMPT_CONSUME_RESUME_SIGNALS
	/* the 3rd step of powerctrl_do_force_down() focuses on this signal. */
	else if (check_sig_deasserted(signal, value, ESPI_VWIRE_SIGNAL_SUS_PWRDN_ACK)) {
		sig_sus_pwrdn_ack_deasserted = 1;
	}
#endif
}

int lom_pwrctrl_result(int* ret)
{
	if (atomic_get(&lom_pwrctrl_busy)) {
		return -EBUSY;
	}

	if (pwrctrl_cmd.processed == 0) {
		return -EAGAIN;
	}

	*ret = pwrctrl_cmd.ret;

	return 0;
}

int lom_pwrctrl_thread_start(void)
{
	k_thread_name_set(lom_pwrctrl_tid, "LOM_MGMT_PWRCTRL");
	k_thread_start(lom_pwrctrl_tid);

	k_sem_take(&lom_pwrctrl_started_sem, K_FOREVER);

	LOG_INF("Power Control thread started");

	return 0;
}

#endif
