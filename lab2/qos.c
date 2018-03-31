#include "rte_common.h"
#include "rte_mbuf.h"
#include "rte_meter.h"
#include "rte_red.h"

#include "qos.h"

#include <assert.h>
#include <stdlib.h>


struct rte_meter_srtcm_params srtcm_params[APP_FLOWS_MAX];
struct rte_meter_srtcm srtcm_data[APP_FLOWS_MAX];
struct rte_red_config red_params[e_RTE_METER_COLORS];
struct rte_red red_data[e_RTE_METER_COLORS];

static const uint64_t clear_time = 1000;

typedef struct queue_node_* queue_node;

struct queue_node_ {
    uint64_t time;
    queue_node next;
};

static struct queue_node_ queue_head;
static queue_node queue = &queue_head;

static queue_node
queue_new_node(uint64_t time)
{
    queue_node p = (queue_node)malloc(sizeof(struct queue_node_));
    p->time = time;
    p->next = NULL;
    return p;
}

static void
queue_push(uint64_t time)
{
    queue_node p = queue;

    while (p->next)
        p = p->next;

    p->next = queue_new_node(time);
}

static unsigned
queue_count(uint64_t time)
{
    queue_node p = queue->next;
    unsigned cnt = 0;

    while (p) {
        if (time - p->time >= clear_time) {
            queue->next = p->next;
            free(p);
            p = queue->next;
        } else {
            ++cnt;
            p = p->next;
        }
    }

    return cnt;
}


/**
 * srTCM
 */
int
qos_meter_init(void)
{
    /* to do */

    int i;
    for (i = 0; i < APP_FLOWS_MAX; ++i) {
        srtcm_params[i].cir = 100;
        srtcm_params[i].cbs = 1200;
        srtcm_params[i].ebs = 2000;
        if (rte_meter_srtcm_config(&srtcm_data[i], &srtcm_params[i]) != 0)
            rte_panic("Cannot init srTCM info\n");
    }

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

    int i;
    for (i = 0; i < e_RTE_METER_COLORS; ++i) {
        if (rte_red_config_init(&red_params[i], 2, 750, 1023, 2) != 0)
            rte_panic("Cannot init RED config\n");

        if (rte_red_rt_data_init(&red_data[i]) != 0)
            rte_panic("Cannot init RED data\n");
    }

    queue_head.time = 0;
    queue_head.next = NULL;

    return 0;
}

int
qos_dropper_run(uint32_t flow_id, enum qos_color color, uint64_t time)
{
    /* to do */

    // Should we use flow_id?
    int result = !!rte_red_enqueue(&red_params[color], &red_data[color], queue_count(time), time);

    if (!result)
        queue_push(time);

    return result;
}
