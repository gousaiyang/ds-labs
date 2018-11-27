# Lab 2: QoS Implementation with DPDK

## Parameter Deduction

### srTCM

|`flow_id`|`cir`|`cbs`|`ebs`|
|-|-|-|-|
|0|160000000000|80000|80000|
|1|80000000000|40000|40000|
|2|40000000000|20000|20000|
|3|20000000000|10000|10000|

- Limit allowance of flow 0 to 160000 bytes in every time period (1000000 ns). So bandwidth is limited to 1.28 Gbps.
- Disperse allowance evenly to two token buckets. (Half green, half yellow, exceed allowance -> red)
- Refill every token bucket after a time period (`cir` is large enough).
- Allowances for 4 flows are in proportion of 8:4:2:1.

### RED

|`color`|`wq_log2`|`min_th`|`max_th`|`maxp_inv`|
|-|-|-|-|-|
|GREEN|9|1022|1023|10|
|YELLOW|9|1022|1023|10|
|RED|9|0|1|10|

- Enqueue as many green/yellow packets as possible. Drop all red packets.
- `wq_log2` and `maxp_inv` are set as recommended in DPDK document.

## Used DPDK APIs

- `rte_panic()`: Used to terminate program when fatal error happens.
- `rte_meter_srtcm_config()`: Used to initialize srTCM data with parameters.
- `rte_red_config_init()`: Used to initialize RED config with parameters.
- `rte_red_rt_data_init()`: Used to initialize RED data.
- `rte_meter_srtcm_color_blind_check()`: Used to mark color for packets.
- `rte_red_mark_queue_empty()`: Used to notify the algorithm about queue clear.
- `rte_red_enqueue()`: Used to make decision whether to enqueue/drop a packet.
