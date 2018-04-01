#include "rte_common.h"
#include "rte_mbuf.h"
#include "rte_meter.h"
#include "rte_red.h"

#include "qos.h"

#include <assert.h>
#include <string.h>


struct rte_meter_srtcm_params srtcm_params[APP_FLOWS_MAX];
struct rte_meter_srtcm srtcm_data[APP_FLOWS_MAX];
struct rte_red_config red_params[e_RTE_METER_COLORS];
struct rte_red red_data[APP_FLOWS_MAX][e_RTE_METER_COLORS];
unsigned queue_size[APP_FLOWS_MAX][e_RTE_METER_COLORS] = {};
uint64_t last_time = 0;

#define SRTCM_CONFIG(flow_id, cir_, cbs_, ebs_) do { \
    srtcm_params[(flow_id)].cir = (cir_); \
    srtcm_params[(flow_id)].cbs = (cbs_); \
    srtcm_params[(flow_id)].ebs = (ebs_); \
    if (rte_meter_srtcm_config(&srtcm_data[(flow_id)], &srtcm_params[(flow_id)]) != 0) \
        rte_panic("Cannot init srTCM data\n"); \
} while (0)

#define RED_CONFIG(color, wq_log2, min_th, max_th, maxp_inv) do { \
    if (rte_red_config_init(&red_params[(color)], (wq_log2), (min_th), (max_th), (maxp_inv)) != 0) \
        rte_panic("Cannot init RED config\n"); \
    int i; \
    for (i = 0; i < APP_FLOWS_MAX; ++i) \
        if (rte_red_rt_data_init(&red_data[i][(color)]) != 0) \
            rte_panic("Cannot init RED data\n"); \
} while (0)


/**
 * srTCM
 */
int
qos_meter_init(void)
{
    /* to do */

    SRTCM_CONFIG(0, 160000000000, 80000, 80000);
    SRTCM_CONFIG(1, 80000000000, 40000, 40000);
    SRTCM_CONFIG(2, 40000000000, 20000, 20000);
    SRTCM_CONFIG(3, 20000000000, 10000, 10000);

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

    RED_CONFIG(GREEN, 9, 1022, 1023, 10);
    RED_CONFIG(YELLOW, 9, 1022, 1023, 10);
    RED_CONFIG(RED, 9, 0, 1, 10);

    return 0;
}

int
qos_dropper_run(uint32_t flow_id, enum qos_color color, uint64_t time)
{
    /* to do */

    assert(flow_id < APP_FLOWS_MAX);

    if (time != last_time) {
        memset(queue_size, 0, sizeof(queue_size));
        int i, j;
        for (i = 0; i < APP_FLOWS_MAX; ++i)
            for (j = 0; j < e_RTE_METER_COLORS; ++j)
                rte_red_mark_queue_empty(&red_data[i][j], time);
    }

    last_time = time;

    int result = !!rte_red_enqueue(&red_params[color], &red_data[flow_id][color], queue_size[flow_id][color], time);

    if (!result)
        ++queue_size[flow_id][color];

    return result;
}
