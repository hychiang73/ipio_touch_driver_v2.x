/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Dicky Chiang <dicky_chiang@ilitek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "ilitek.h"

#define PROTOCOL_VER_500    0x050000
#define PROTOCOL_VER_510    0x050100
#define PROTOCOL_VER_520    0x050200
#define PROTOCOL_VER_530    0x050300
#define PROTOCOL_VER_540    0x050400
#define PROTOCOL_VER_550    0x050500
#define PROTOCOL_VER_560    0x050600

#define TDDI_PID_ADDR           0x4009C
#define TDDI_OTP_ID_ADDR		0x400A0
#define TDDI_ANA_ID_ADDR        0x400A4
#define TDDI_PC_COUNTER_ADDR    0x44008
#define TDDI_WDT_ADDR           0x5100C
#define TDDI_CHIP_RESET_ADDR    0x40050

#define ILI9881H_CHIP       0x9881
#define ILI7807G_CHIP       0x7807

#define CHIP_SUP_NUM        2
static u16 ic_sup_list[CHIP_SUP_NUM] = {
    [0] = ILI9881H_CHIP,
    [1] = ILI7807G_CHIP
};

#define PROTOCL_VER_NUM     7
static struct ilitek_protocol_info protocol_info[PROTOCL_VER_NUM] = {
    /* length -> fw, protocol, tp, key, panel, core, func, window, cdc, mp_info */
    [0] = {PROTOCOL_VER_500, 4, 4, 14, 30, 5, 5, 2, 3, 8},
    [1] = {PROTOCOL_VER_510, 4, 3, 14, 30, 5, 5, 3, 3, 8},
    [2] = {PROTOCOL_VER_520, 4, 4, 14, 30, 5, 5, 3, 3, 8},
    [3] = {PROTOCOL_VER_530, 9, 4, 14, 30, 5, 5, 3, 3, 8},
    [4] = {PROTOCOL_VER_540, 9, 4, 14, 30, 5, 5, 3, 15, 8},
    [5] = {PROTOCOL_VER_550, 9, 4, 14, 30, 5, 5, 3, 15, 14},
    [6] = {PROTOCOL_VER_560, 9, 4, 14, 30, 5, 5, 3, 15, 14},
};

#define FUNC_CTRL_NUM   14
static struct ilitek_ic_func_ctrl func_ctrl[FUNC_CTRL_NUM] = {
    /* cmd[3] = cmd, func, ctrl */
    [0] = {"sense", {0x1,0x1,0x0}, 3},
    [1] = {"sleep", {0x1,0x2,0x0}, 3},
    [2] = {"glove", {0x1,0x6,0x0}, 3},
    [3] = {"stylus", {0x1,0x7,0x0}, 3},
    [4] = {"tp_scan_mode", {0x1,0x8,0x0}, 3},
    [5] = {"lpwg", {0x1,0xA,0x0}, 3},
    [6] = {"gesture", {0x1,0xB,0x3F}, 3},
    [7] = {"phone_cover", {0x1,0xC,0x0}, 3},
    [8] = {"finger_sense", {0x1,0xF,0x0}, 3},
    [9] = {"phone_cover_window", {0xE,0x0,0x0}, 3},
    [10] = {"proximity", {0x1,0x10,0x0}, 3},
    [11] = {"edge_palm", {0x1,0x12,0x0}, 3},
    [12] = {"lock_point", {0x1,0x13,0x0}, 3},
    [13] = {"active", {0x1,0x14,0x0}, 3},
};

static int ilitek_ice_mode_write(struct ilitek_tddi_dev *idev, u32 addr, u32 data, size_t len)
{
    int ret = 0, i;
    u8 txbuf[64] = {0};

    if (atomic_read(&idev->ice_stat) == IRQ_DISABLE) {
        ipio_err("ICE Mode isn't enabled\n");
        return -1;
    }

    txbuf[0] = 0x25;
	txbuf[1] = (char)((addr & 0x000000FF) >> 0);
	txbuf[2] = (char)((addr & 0x0000FF00) >> 8);
	txbuf[3] = (char)((addr & 0x00FF0000) >> 16);

	for (i = 0; i < len; i++)
		txbuf[i + 4] = (char)(data >> (8 * i));

    ret = idev->write(idev, txbuf, len + 4);
    if (ret < 0)
        ipio_err("Failed to write data in ice mode, ret = %d\n", ret);

    return ret;
}

static int ilitek_ice_mode_read(struct ilitek_tddi_dev *idev, u32 addr, u32 *data)
{
    int ret = 0;
    u8 txbuf[64] = {0};

    if (atomic_read(&idev->ice_stat) == IRQ_DISABLE) {
        ipio_err("ICE Mode isn't enabled\n");
        return -1;
    }

	txbuf[0] = 0x25;
	txbuf[1] = (char)((addr & 0x000000FF) >> 0);
	txbuf[2] = (char)((addr & 0x0000FF00) >> 8);
	txbuf[3] = (char)((addr & 0x00FF0000) >> 16);

	ret = idev->write(idev, txbuf, 4);
	if (ret < 0)
        goto out;

    msleep(1);

    memset(txbuf, 0, sizeof(txbuf));

    ret = idev->read(idev, txbuf, 4);
    if (ret < 0)
        goto out;

    *data = (txbuf[0] | txbuf[1] << 8 | txbuf[2] << 16 | txbuf[3] << 24);

out:
    if (ret < 0)
        ipio_err("Failed to read data in ice mode, ret = %d\n", ret);

    return ret;
}

int ilitek_ice_mode_ctrl(struct ilitek_tddi_dev *idev, bool enable, bool mcu)
{
    int ret = 0, count = 3;
    u8 cmd_open[4] = {0x25, 0x62, 0x10, 0x18};
    u8 cmd_close[4] = {0x1B, 0x62, 0x10, 0x18};

    if (enable) {
        if (!mcu)
            cmd_open[0] = 0x1F;

        atomic_set(&idev->ice_stat, ICE_ENABLE);
retry:
        ret = idev->write(idev, cmd_open, sizeof(cmd_open));
        if (ret < 0)
            ipio_err("Enter to ICE Mode failed\n");

        mdelay(25);

        /* Check if ice mode is enabled successfully */
        ret = ilitek_tddi_ic_get_info(idev);
        if (ret < 0 && count != 0) {
            count--;
            goto retry;
        }
    } else {
        ret = idev->write(idev, cmd_close, sizeof(cmd_close));
        if (ret < 0)
            ipio_err("Exit to ICE Mode failed\n");

        atomic_set(&idev->ice_stat, ICE_DISABLE);
    }

    return ret;
}

int ilitek_tddi_ic_func_ctrl(struct ilitek_tddi_dev *idev, const char *name, int ctrl)
{
    int i = 0;

    for (i = 0; i < FUNC_CTRL_NUM; i++) {
        if (strncmp(name, func_ctrl[i].name, strlen(name)) == 0) {
            if (!CHECK_EQUAL(strlen(name), strlen(func_ctrl[i].name)))
                continue;
            break;
        }
    }

    if (i >= FUNC_CTRL_NUM) {
        ipio_err("Not found func ctrl, %s\n", name);
        return -1;
    }

    if (idev->protocol->ver == PROTOCOL_VER_500) {
        ipio_err("Non support func ctrl with protocol v5.0\n");
        return -1;
    }

    if (idev->protocol->ver >= PROTOCOL_VER_560) {
        if (strncmp(func_ctrl[i].name, "gesture", strlen("gesture")) == 0 ||
            strncmp(func_ctrl[i].name, "phone_cover_window", strlen("phone_cover_window")) == 0) {
            ipio_info("Non support %s ctrl\n", func_ctrl[i].name);
            return 0;
        }
    }

    if (ctrl == ENABLE || ctrl == ON)
        func_ctrl[i].cmd[2] = 0x1;
    else
        func_ctrl[i].cmd[2] = 0x0;

    ipio_info("func = %s, len = %d, cmd = 0x%x, 0%x, 0x%x\n", func_ctrl[i].name, func_ctrl[i].len,
        func_ctrl[i].cmd[0], func_ctrl[i].cmd[1], func_ctrl[i].cmd[2]);

    return idev->write(idev, func_ctrl[i].cmd, func_ctrl[i].len);;
}

u32 ilitek_tddi_ic_get_pc_counter(struct ilitek_tddi_dev *idev)
{
    bool ice = ICE_DISABLE;
    u32 pc = 0;

    ice = atomic_read(&idev->ice_stat);

    if (ice == ICE_DISABLE)
        ilitek_ice_mode_ctrl(idev, ICE_ENABLE, MCU_STOP);

    idev->chip->ice_read(idev, idev->chip->pc_counter_addr, &pc);

    ipio_info("pc counter = 0x%x\n", pc);

    if (ice == ICE_DISABLE)
        ilitek_ice_mode_ctrl(idev, ICE_DISABLE, MCU_STOP);

    return pc;
}

int ilitek_tddi_ic_check_busy(struct ilitek_tddi_dev *idev, int count, int delay)
{
    u8 cmd[2] = {0};
    u8 busy = 0, rby = 0;

    cmd[0] = P5_X_READ_DATA_CTRL;
    cmd[1] = P5_X_CDC_BUSY_STATE;

    if (idev->actual_fw_mode == P5_X_FW_DEMO_MODE)
        rby = 0x41;
    else if (idev->actual_fw_mode == P5_X_FW_TEST_MODE)
        rby = 0x51;
    else {
        ipio_err("Unknown fw mode (0x%x)\n", idev->actual_fw_mode);
        return -EINVAL;
    }

    ipio_info("read byte = %x, delay = %d\n", rby, delay);

    do {
        idev->write(idev, cmd, sizeof(cmd));
        idev->write(idev, &cmd[1], sizeof(u8));
        idev->read(idev, &busy, sizeof(u8));

        ipio_info("busy = 0x%x\n", busy);

        if (CHECK_EQUAL(busy, rby)) {
            ipio_info("Check busy free\n");
            goto out;
        }

        mdelay(delay);
    } while (--count > 0);

    ipio_err("Check busy (%x) timeout !, pc = 0x%x\n", busy,
        ilitek_tddi_ic_get_pc_counter(idev));
    return -1;

out:
    return 0;
}

int ilitek_tddi_ic_get_fw_ver(struct ilitek_tddi_dev *idev)
{
    int ret = 0;
    u8 cmd[2] = {0};
    u8 buf[10] = {0};

    cmd[0] = P5_X_READ_DATA_CTRL;
    cmd[1] = P5_X_GET_FW_VERSION;

    ret = idev->write(idev, cmd, sizeof(cmd));
    if (ret < 0) {
        ipio_err("write firmware ver err\n");
        goto out;
    }

    ret = idev->write(idev, &cmd[1], sizeof(u8));
    if (ret < 0) {
        ipio_err("write firmware ver err\n");
        goto out;
    }

    ret = idev->read(idev, buf, idev->protocol->fw_ver_len);
    if (ret < 0) {
        ipio_err("i2c/spi read firmware ver err\n");
        goto out;
    }

    ilitek_dump_data(buf, 8, idev->protocol->fw_ver_len, 0, "firmware ver");

    if (buf[0] != P5_X_GET_FW_VERSION) {
        ipio_err("Invalid firmware ver\n");
        ret = -EINVAL;
        goto out;
    }

    ipio_info("Firmware version = %d.%d.%d.%d\n", buf[1], buf[2], buf[3], buf[4]);

    idev->chip->fw_ver = buf[1] << 24 | buf[2] << 16 | buf[3] << 8 | buf[4];

out:
    return ret;
}

int ilitek_tddi_ic_get_panel_info(struct ilitek_tddi_dev *idev)
{
    int ret = 0;
    u8 cmd = P5_X_GET_PANEL_INFORMATION;
    u8 buf[10] = {0};

    ret = idev->write(idev, &cmd, sizeof(u8));
    if (ret < 0) {
        ipio_err("Write panel info error\n");
        goto out;
    }

    ret = idev->read(idev, buf, idev->protocol->panel_info_len);
    if (ret < 0) {
        ipio_err("Read panel info error\n");
        goto out;
    }

    ilitek_dump_data(buf, 8, idev->protocol->panel_info_len, 0, "Panel info");

out:
    if (buf[0] != P5_X_GET_PANEL_INFORMATION) {
        idev->panel_wid = TOUCH_SCREEN_X_MAX;
        idev->panel_hei = TOUCH_SCREEN_Y_MAX;
    } else {
        idev->panel_wid = buf[1] << 8 | buf[2];
        idev->panel_hei = buf[3] << 8 | buf[4];
    }

    ipio_info("Panel info: width = %d, height = %d\n", idev->panel_wid, idev->panel_hei);
    return ret;
}

int ilitek_tddi_ic_get_tp_info(struct ilitek_tddi_dev *idev)
{
    int ret = 0;
    u8 cmd[2] = {0};
    u8 buf[20] = {0};

    cmd[0] = P5_X_READ_DATA_CTRL;
    cmd[1] = P5_X_GET_TP_INFORMATION;

    ret = idev->write(idev, cmd, sizeof(cmd));
    if (ret < 0) {
        ipio_err("Write tp info error\n");
        goto out;
    }

    ret = idev->write(idev, &cmd[1], sizeof(u8));
    if (ret < 0) {
        ipio_err("Write tp info error\n");
        goto out;
    }

    ret = idev->read(idev, buf, idev->protocol->tp_info_len);
    if (ret < 0) {
        ipio_err("Read tp info error\n");
        goto out;
    }

    ilitek_dump_data(buf, 8, idev->protocol->tp_info_len, 0, "TP info");

    if (buf[0] != P5_X_GET_TP_INFORMATION) {
        ipio_err("Invalid tp info\n");
        ret = -EINVAL;
        goto out;
    }

    idev->min_x = buf[1];
    idev->min_y = buf[2];
    idev->max_x = buf[4] << 8 | buf[3];
    idev->max_y = buf[6] << 8 | buf[5];
    idev->xch_num = buf[7];
    idev->ych_num = buf[8];

    ipio_info("TP Info: min_x = %d, min_y = %d, max_x = %d, max_y = %d, xch = %d, ych = %d\n",
        idev->min_x, idev->min_y, idev->max_x, idev->max_y, idev->xch_num, idev->ych_num);

out:
    return ret;
}

static void ilitek_tddi_ic_check_protocol_ver(struct ilitek_tddi_dev *idev, u32 pver)
{
    int i = 0;

    if (CHECK_EQUAL(idev->protocol->ver, pver)) {
        ipio_info("same procotol version, do nothing\n");
        return;
    }

    for (i = 0; i < PROTOCL_VER_NUM - 1; i++) {
        if (CHECK_EQUAL(protocol_info[i].ver, pver)) {
            idev->protocol = &protocol_info[i];
            ipio_info("update protocol version = %x\n", idev->protocol->ver);
            return;
        }
    }

    ipio_info("Not found a correct protocol version in list, use newest version\n");
    idev->protocol = &protocol_info[PROTOCL_VER_NUM - 1];
}

int ilitek_tddi_ic_get_protocl_ver(struct ilitek_tddi_dev *idev)
{
    int ret = 0;
    u8 cmd[2] = {0};
    u8 buf[10] = {0};
    u32 ver;

    cmd[0] = P5_X_READ_DATA_CTRL;
    cmd[1] = P5_X_GET_PROTOCOL_VERSION;

    ret = idev->write(idev, cmd, sizeof(cmd));
    if (ret < 0) {
        ipio_err("Write protocol version error\n");
        goto out;
    }

    ret = idev->write(idev, &cmd[1], sizeof(u8));
    if (ret < 0) {
        ipio_err("Write protocol version error\n");
        goto out;
    }

    ret = idev->read(idev, buf, idev->protocol->pro_ver_len);
    if (ret < 0) {
        ipio_err("Read protocol version error\n");
        goto out;
    }

    ilitek_dump_data(buf, 8, idev->protocol->pro_ver_len, 0, "protocol ver");

    if (buf[0] != P5_X_GET_PROTOCOL_VERSION) {
        ipio_err("Invalid protocol ver\n");
        ret = -EINVAL;
        goto out;
    }

    ver = buf[1] << 16 | buf[2] << 8 | buf[3];

    ilitek_tddi_ic_check_protocol_ver(idev, ver);

    ipio_info("Protocol version = %d.%d.%d\n", idev->protocol->ver >> 16,
        (idev->protocol->ver >> 8) & 0xFF, idev->protocol->ver & 0xFF);

out:
    return ret;
}

int ilitek_tddi_ic_get_info(struct ilitek_tddi_dev *idev)
{
    int i, ret = 0, ice = 0;

    ice = atomic_read(&idev->ice_stat);

    if (ice == ICE_DISABLE)
        ilitek_ice_mode_ctrl(idev, ICE_ENABLE, MCU_STOP);

    idev->chip->ice_read(idev, idev->chip->pid_addr, &idev->chip->pid);
    idev->chip->id = idev->chip->pid >> 16;
    idev->chip->type_hi = idev->chip->pid & 0x0000FF00;
    idev->chip->type_low = idev->chip->pid  & 0xFF;

    ipio_info("CHIP PID = 0x%x\n", idev->chip->pid);
	ipio_info("CHIP ID = 0x%x\n", idev->chip->id);
	ipio_info("CHIP Type = 0x%x\n", (idev->chip->type_hi << 8) | idev->chip->type_low);

	idev->chip->ice_read(idev, idev->chip->otp_addr, &idev->chip->otp_id);
	idev->chip->ice_read(idev, idev->chip->ana_addr, &idev->chip->ana_id);

    idev->chip->otp_id &= 0xFF;
    idev->chip->ana_id &= 0xFF;

	ipio_info("CHIP OTP ID = 0x%x\n", idev->chip->otp_id);
	ipio_info("CHIP ANA ID = 0x%x\n", idev->chip->ana_id);

    for (i = 0; i < CHIP_SUP_NUM; i++)
        if (CHECK_EQUAL(idev->chip->id, ic_sup_list[i]))
            break;

    if (i >= CHIP_SUP_NUM)
        ret = -1;

    if (ice == ICE_DISABLE)
	    ilitek_ice_mode_ctrl(idev, ICE_DISABLE, MCU_STOP);

    return ret;
}

int ilitek_tddi_ic_init(struct ilitek_tddi_dev *idev)
{
    struct ilitek_ic_info *chip;

    chip = devm_kzalloc(idev->dev, sizeof(struct ilitek_ic_info), GFP_KERNEL);
    if (ERR_ALLOC_MEM(chip)) {
        ipio_err("Failed to allocate idev memory, %ld\n", PTR_ERR(chip));
        return -ENOMEM;
    }

    chip->ice_write =       ilitek_ice_mode_write;
    chip->ice_read =        ilitek_ice_mode_read;

    chip->pid_addr =        TDDI_PID_ADDR;
    chip->wdt_addr =        TDDI_WDT_ADDR;
    chip->pc_counter_addr = TDDI_PC_COUNTER_ADDR;
    chip->otp_addr =        TDDI_OTP_ID_ADDR;
    chip->ana_addr =        TDDI_ANA_ID_ADDR;
    chip->reset_addr =      TDDI_CHIP_RESET_ADDR;

    idev->protocol = &protocol_info[PROTOCL_VER_NUM - 1];
    idev->chip = chip;
    return 0;
}