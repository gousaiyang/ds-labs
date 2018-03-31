#include "rte_common.h"
#include "rte_mbuf.h"
#include "rte_meter.h"
#include "rte_red.h"

#include "qos.h"

#include <assert.h>
#include <string.h>


struct rte_meter_srtcm_params srtcm_params[APP_FLOWS_MAX];
struct rte_meter_srtcm srtcm_data[APP_FLOWS_MAX];
struct rte_red_config red_params[APP_FLOWS_MAX][e_RTE_METER_COLORS];
struct rte_red red_data[APP_FLOWS_MAX][e_RTE_METER_COLORS];

#define SRTCM_CONFIG(flow_id, cir_, cbs_, ebs_) do { \
    srtcm_params[(flow_id)].cir = (cir_); \
    srtcm_params[(flow_id)].cbs = (cbs_); \
    srtcm_params[(flow_id)].ebs = (ebs_); \
    if (rte_meter_srtcm_config(&srtcm_data[(flow_id)], &srtcm_params[(flow_id)]) != 0) \
        rte_panic("Cannot init srTCM data\n"); \
} while (0)

#define RED_CONFIG(flow_id, color, wq_log2, min_th, max_th, maxp_inv) do { \
    if (rte_red_config_init(&red_params[(flow_id)][(color)], (wq_log2), (min_th), (max_th), (maxp_inv)) != 0) \
        rte_panic("Cannot init RED config\n"); \
    if (rte_red_rt_data_init(&red_data[(flow_id)][(color)]) != 0) \
        rte_panic("Cannot init RED data\n"); \
} while (0)


uint64_t last_time = 0;
unsigned queue_size[APP_FLOWS_MAX] = {};


/**
 * srTCM
 */
int
qos_meter_init(void)
{
    /* to do */

    SRTCM_CONFIG(0, 2000, 1152, 1152);
    SRTCM_CONFIG(1, 2000, 640, 640);
    SRTCM_CONFIG(2, 2000, 384, 384);
    SRTCM_CONFIG(3, 2000, 256, 256);

    return 0;
}

enum qos_color
qos_meter_run(uint32_t flow_id, uint32_t pkt_len, uint64_t time)
{
    /* to do */
    assert(flow_id < APP_FLOWS_MAX);

    return rte_meter_srtcm_color_blind_check(&srtcm_data[flow_id], time, pkt_len);
}


/**
 * WRED
 */

int
qos_dropper_init(void)
{
    /* to do */

    RED_CONFIG(0, GREEN, 12, 1022, 1023, 255);
    RED_CONFIG(0, YELLOW, 12, 1, 2, 1);
    RED_CONFIG(0, RED, 12, 1, 2, 1);
    RED_CONFIG(1, GREEN, 12, 1022, 1023, 255);
    RED_CONFIG(1, YELLOW, 12, 1, 2, 1);
    RED_CONFIG(1, RED, 12, 1, 2, 1);
    RED_CONFIG(2, GREEN, 12, 1022, 1023, 255);
    RED_CONFIG(2, YELLOW, 12, 1, 2, 1);
    RED_CONFIG(2, RED, 12, 1, 2, 1);
    RED_CONFIG(3, GREEN, 12, 1022, 1023, 255);
    RED_CONFIG(3, YELLOW, 12, 1, 2, 1);
    RED_CONFIG(3, RED, 12, 1, 2, 1);

    return 0;
}

int
qos_dropper_run(uint32_t flow_id, enum qos_color color, uint64_t time)
{
    /* to do */

    if (time != last_time)
        memset(queue_size, 0, sizeof(queue_size));

    int result = !!rte_red_enqueue(&red_params[flow_id][color], &red_data[flow_id][color], queue_size[flow_id], time);

    if (!result)
        ++queue_size[flow_id];

    return result;
}
