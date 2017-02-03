#ifndef PLR_MMC_POWEROFF_H
#define PLR_MMC_POWEROFF_H
enum mmc_poweroff_state {
    STATE_READY = 0,
    STATE_POWERON,
    STATE_POWEROFF,
    STATE_POWEROFF_READY,
    STATE_POWEROFF_STOP,
    STATE_POWEROFF_TIMER
};

enum mmc_poweroff_cmd {
    CMD_READY = 0,
    CMD_POWEROFF,
    CMD_POWERON,
    CMD_POWEROFF_STOP
};

struct mmc_poweroff_cond {
    int excute_internal;
    int busy;
    int command;
    int ret;
};

#endif
