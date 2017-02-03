
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <stdlib.h>
#include <dirent.h>
#include <limits.h>

#include <fcntl.h>
#include <termios.h>
#include <termio.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <linux/fs.h>
#include <sys/mman.h>
#include <time.h>
#include <sys/select.h>

#include "plr_system.h"
#include "plr_protocol.h"
#include "plr_common.h"
#include "plr_tftp.h"
#include "plr_mmc_poweroff.h"

#define BLKDISCARD _IO(0x12,119)
#define BLKSECDISCARD	_IO(0x12,125)

#define BYTE_1G		(uint64_t)(1073741824)
#define SEC_1G		BYTE2SEC(BYTE_1G)
#define MB (1024 * 1024)

#define timespecsub(a, b, result) \
do { \
	(result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
	(result)->tv_nsec = (a)->tv_nsec - (b)->tv_nsec; \
	if ((result)->tv_nsec < 0) { \
		--(result)->tv_sec; \
		(result)->tv_nsec += 1000000000; \
	} \
} while (0)

#ifndef O_DIRECT
#ifndef __O_DIRECT
#define O_DIRECT	0200000 /* Direct disk access.  */
#else
#define O_DIRECT 	__O_DIRECT
#endif
#endif

char console_buffer[CONFIG_SYS_CBSIZE + 1];
extern struct plr_state g_plr_state;
extern struct tftp_state g_tftp_state;

static char _kbhit (void)
{
	
	struct termio stTerm;
	u_short usFlag;
	u_char uchMin;
	u_char uchTime;
	char ch = 0;
	int retry = 100;

	ioctl( 0, TCGETA, &stTerm);

	usFlag = stTerm.c_lflag;
	uchMin = stTerm.c_cc[VMIN];
	uchTime = stTerm.c_cc[VTIME];

	stTerm.c_lflag &= ~ICANON;

	stTerm.c_cc[VMIN] = 0;
	stTerm.c_cc[VTIME] = 0;

	ioctl(0, TCSETA, &stTerm);
	
#ifndef	PLR_SYSTEM
	while (read(0, &ch, 1) < 1);
#else
	while (read(0, &ch, 1) < 1){
		mdelay(10);
		if (retry-- < 0){
			continue;
		}
		else
			return (char)-1;
	}
#endif
//	read(0, &ch, 1);

	stTerm.c_lflag = usFlag;
	stTerm.c_cc[VMIN] = uchMin;
	stTerm.c_cc[VTIME] = uchTime;
	ioctl(0, TCSETA, &stTerm);

	return ch;
}

static int linux_kbhit(void)
{
	struct termios oldt, newt;
	int ch;
	int oldf;
	
	tcgetattr(STDIN_FILENO, &oldt);

	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);

	tcsetattr(STDIN_FILENO, TCSANOW, &newt);

	oldf = fcntl(STDIN_FILENO, F_GETFL, 0);

	fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

	ch = getchar();

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

	fcntl(STDIN_FILENO, F_SETFL, oldf);

	if(ch != EOF)
	{
		ungetc(ch, stdin);
		return 1;
	}

	return 0;
}


static int readline_into_buffer (char * buffer)
{
	char *p = buffer;
	char *p_buf = buffer;
	int	n = 0;				/* buffer index		*/
	char c;
#ifdef PLR_SYSTEM	//160126 retry 및 err처리추가
	int retry = 10;
#endif

	for (;;) {
		c = _kbhit();
	//	printf ("!!! %d\n", c);
#ifdef PLR_SYSTEM	//160126 retry 및 err처리 추가
		if (c == (char)-1){
			if (retry-- > 0)
				continue;
			else
				return -1;
		}
		else
			retry = 10;
#endif

		/*
		 * Special character handling
		 */
		switch (c) {
		case '\r':				/* Enter		*/
		case '\n':
			*p = '\0';
			return (p - p_buf);

		case '\0':				/* nul			*/
			continue;

		case 0x03:				/* ^C - break		*/
			p_buf[0] = '\0';	/* discard input */
			return (-1);

		default:
			/*
			 * Must be a normal character then
			 */
			if (n < CONFIG_SYS_CBSIZE-2) {
				*p++ = c;
				++n;
			}
		}
	}
}

int safe_write(int fd, const void *buf, size_t count)
{
	ssize_t n;

	do {
		n = write(fd, buf, count);
	} while (n < 0 && errno == EINTR);

	return n;
}

uint64_t get_block_total_size(int fd)
{
	uint64_t total = 0;
		
	if (ioctl(fd, BLKGETSIZE64, &total)) {
		plr_err ("ioctl BLKGETSIZE64 error!!!\n");
		return 0;
	}
	return total;
}

/* -----------------------------------------------------------------------------
 * common api 
 * ---------------------------------------------------------------------------- */

int readline (const char *const prompt)
{
	int ret = 0;
	console_buffer[0] = '\0';

	ret =  readline_into_buffer(console_buffer);
	fflush(stdin);
	return ret;
}

unsigned long simple_strtoul(const char *cp,char **endp,unsigned int base)
{
	unsigned long result = 0,value;

	if (*cp == '0') {
		cp++;
		if ((*cp == 'x') && isxdigit(cp[1])) {
			base = 16;
			cp++;
		}
		if (!base) {
			base = 8;
		}
	}
	if (!base) {
		base = 10;
	}
	while (isxdigit(*cp) && (value = isdigit(*cp) ? *cp-'0' : (islower(*cp)
	    ? toupper(*cp) : *cp)-'A'+10) < base) {
		result = result*base + value;
		cp++;
	}
	if (endp)
		*endp = (char *)cp;
	return result;
}

int dev_ext4_mount(char *dev_name, char* test_path)
{
	char cmd[128] = {0};
	DIR *dirp = NULL;
	int ret = 0;

	dirp = opendir(test_path);

	if(dirp == NULL) {
		system("mount -o rw,remount / /");
		sprintf (cmd, "mkdir %s", test_path);
		ret = system(cmd);
		if (ret) {
			printf("Make test foler %s fail \n", test_path);
			return -1;
		}
		system("mount -o ro,remount / /");
	}

	if (dirp)
		closedir(dirp);

	sprintf (cmd, "umount %s", test_path);
	system(cmd);
	
	sprintf (cmd, "mount -t ext4 -o nosuid,nodev,journal_checksum %s %s", dev_name, test_path);
	ret = system(cmd);
	
	return ret;
}

int dev_get_size(char *dev_name, unsigned long long *size)
{
        int fd;

	fd = open(dev_name, O_RDWR | O_DIRECT);

	if (ioctl(fd, BLKGETSIZE64, size)) {
                perror(dev_name);
		return -1;
	}

	close(fd);

        return 0;
}

int dev_erase(uint dev_num, uint start_sector, uint len, int type) 
{
	int ret = 0;
	int fd = dev_num;
	int _type;
	uint64_t total = 0, range[2] = {0};
	uint write_sec = start_sector, end_sec = start_sector+len;
	uint64_t secsize = 0;

	plr_info ("Start erase %s\n", MMC_PATH);
	
	if (fd == mmc_fd){
		if (ioctl(fd, BLKGETSIZE64, &total)) {
			plr_err ("%s ioctl BLKGETSIZE64 error!!!\n", MMC_PATH);
			return -1;
		}

		if (ioctl(fd, BLKSSZGET, &secsize)) {
			plr_err ("%s ioctl BLKSSZGET error!!!\n", MMC_PATH);
			return -1;
		}
	}
	else if (fd == sd_state_fd){
		plr_err("unsupported sd_state erase..\n");
		return -1;
	}
	else {
		plr_err("unknown fd... mmc fd: %d, sd fd: %d, current fd : %d\n",
				mmc_fd, sd_state_fd, fd);
		return -1;
	}

	if (total < (uint64_t)SEC2BYTE(end_sec)){
		plr_err("erase sector length over. start : %u, len : %u, total : %llu\n", start_sector, len, total);
		return -1;
	}

	switch (type){
		case TYPE_SANITIZE:
			_type = BLKSECDISCARD;
			break;
		case TYPE_ERASE:
		case TYPE_TRIM:
		case TYPE_DISCARD:
			_type = BLKDISCARD;
			break;
		default:
			plr_err("unknown erase type..%d\n", type);
			return -1;
			break;
	}
	/* try secure discard */
	range[0] = (uint64_t)SEC2BYTE(write_sec);
	range[0] = (range[0] + secsize - 1) & ~(secsize - 1);
	range[1] = BYTE_1G;
	while (write_sec+SEC_1G <= end_sec){
		ret = ioctl(fd, _type, &range);
		/*

		if (ret)
			ret = ioctl(fd, BLKDISCARD, &range);
		*/
		if (ret) {
			plr_err ("%s BLKDISCARD ioctl error %d!!!\n", MMC_PATH, ret);
			plr_err ("Not support BLKDISCARD ioctl\n");
			return -1;
		}
		plr_info("\r%4uGB/%4uGB", write_sec/SEC_1G, end_sec/SEC_1G);
		write_sec += SEC_1G;
		range[0] = (uint64_t)SEC2BYTE(write_sec);
		range[0] = (range[0] + secsize - 1) & ~(secsize - 1);
	}
	range[1] = (uint64_t)SEC2BYTE(end_sec-write_sec);

	/* align range to the sector size */	
	range[1] &= ~((uint64_t)secsize - 1);

	ret = ioctl(fd, _type, &range);
	if (ret) {
		plr_err ("%s erase ioctl error %d!!!\n", MMC_PATH, ret);
		return -1;
	}
	plr_info("\r%4uGB/%4uGB\n", write_sec/SEC_1G, end_sec/SEC_1G);

	fsync(fd);
	return ret;
}
 
uint crc32 (uint crc, const unsigned char *buf, uint len)
{
	static unsigned int table[256];
	static int have_table = 0;
	unsigned int rem;
	uint8_t octet;
	int i, j;
	const unsigned char *p, *q;
 
	/* This check is not thread safe; there is no mutex. */
	if (have_table == 0) {
		/* Calculate CRC table. */
		for (i = 0; i < 256; i++) {
			rem = i;  /* remainder from polynomial division */
			for (j = 0; j < 8; j++) {
				if (rem & 1) {
					rem >>= 1;
					rem ^= 0xedb88320;
				} else
					rem >>= 1;
			}
			table[i] = rem;
		}
		have_table = 1;
	}
 
	crc = ~crc;
	q = buf + len;
	for (p = buf; p < q; p++) {
		octet = *p;  /* Cast to unsigned octet. */
		crc = (crc >> 8) ^ table[(crc & 0xff) ^ octet];
	}
	return ~crc;
}

char *g_read_buf_addr;
char *g_write_buf_addr;
char *g_extra_buf_addr;
char *g_mmc_cond_buf_addr;
//char *g_mmc_data_buf_addr;
//internal_info_t *g_mmc_data_buf_addr;
//int g_fd = 0;

int mmc_fd;
int sd_state_fd;
int sw_reset_fd;

int prepare_buffers(void)
{
	g_read_buf_addr = (char *)valloc(PAGE_ALIGN(READ_BUF_SZ));
	if (g_read_buf_addr == NULL){
		plr_err("read buff create fail\n");
		perror("");
		return -1;
	}

	g_write_buf_addr = (char *)valloc(PAGE_ALIGN(WRITE_BUF_SZ));
	if (g_write_buf_addr == NULL){
		plr_err("read buff create fail\n");
		perror("");
		return -1;
	}

	g_extra_buf_addr = (char *)valloc(PAGE_ALIGN(EXTRA_BUF_SZ));
	if (g_extra_buf_addr == NULL){
		plr_err("read buff create fail\n");
		perror("");
		return -1;
	}

	g_mmc_cond_buf_addr = (char *)malloc(sizeof(struct mmc_poweroff_cond));
	if (g_mmc_cond_buf_addr == NULL){
		plr_err("read buff create fail\n");
		perror("");
		return -1;
	}

	memset(g_read_buf_addr, 0, PAGE_ALIGN(READ_BUF_SZ));
	memset(g_write_buf_addr, 0, PAGE_ALIGN(WRITE_BUF_SZ));
	memset(g_extra_buf_addr, 0, PAGE_ALIGN(EXTRA_BUF_SZ));
	memset(g_mmc_cond_buf_addr, 0, sizeof(struct mmc_poweroff_cond));

	return 0;
}

void free_buffers(void)
{
	free(g_read_buf_addr);
	free(g_write_buf_addr);
	free(g_extra_buf_addr);
	free(g_mmc_cond_buf_addr);

	g_read_buf_addr = NULL;
	g_write_buf_addr = NULL;
	g_extra_buf_addr = NULL;
	g_mmc_cond_buf_addr = NULL;
}

int prepare_other_devices(void)
{
	int fd;

	if (sd_state_fd != 0){
		plr_err("already open sd_state %d\n", sd_state_fd);
		return -1;
	}


	fd = open(SD_STATE_PATH, O_RDWR|O_LARGEFILE|O_CREAT,0666);
	if (fd < 0){
		perror("sd_state open fail");
		printf ("open err : %s\n", SD_STATE_PATH);
		return -1;
	}

	sd_state_fd = fd;

	if (sw_reset_fd != 0){
		plr_err("already open sw_reset_fd %d\n", sw_reset_fd);
		return -1;
	}

	fd = open(SW_RESET_PATH, O_RDWR|O_LARGEFILE|O_CREAT,0666);
	if (fd < 0){
		perror("sw_reset_ open fail");
		printf ("open err : %s\n", SW_RESET_PATH);
		return -1;
	}
	sw_reset_fd = fd;

	return 0;
}

int prepare_mmc(void)
{
	int fd;

	if (mmc_fd != 0){
		plr_err("already open mmc %d\n", mmc_fd);
		return 0;
	}

	fd = open(MMC_PATH, O_RDWR|O_LARGEFILE|O_DIRECT);
	if (fd < 0){
		perror("mmc open fail");
		return -1;
	}

	mmc_fd = fd;
	
	return 0;
}

int close_mmc(void)
{
	if (!mmc_fd){
		plr_err("alread close mmc\n");
		return 0;
	}

	if (close(mmc_fd)){
		plr_err("mmc close fail : ");
		perror("");
		return -1;
	}
	mmc_fd = 0;

	return 0;
}

int close_other_devices(void)
{
	if (close(sd_state_fd)){
		plr_err("sd state close fail :");
		perror("");
		return -1;
	}
	sd_state_fd = 0;

	if (close(sw_reset_fd)){
		plr_err("sd state close fail :");
		perror("");
		return -1;
	}
    sw_reset_fd = 0;

	return 0;
}

int poweroff_data_check(void)
{
#if 1
	int ret = 0;
	int fd = 0;
	int cnt = 5;
	internal_info_t recv_info;

	memset (&recv_info, 0, sizeof(internal_info_t));

	
	while (cnt--){
		fd = open(DEVICE_POWEROFF_DATA_PATH, O_RDWR|O_SYNC);

		if (fd < 0) {
			plr_err("[%s] Open internal info fail\n", __func__);
			perror("");
			return -1;
		}
		
		ret = read(fd, &recv_info, sizeof(internal_info_t));
		
		if (ret < 0){
			plr_err("[%s] Read internal info fail\n", __func__);
			perror("");
			return -1;
		}
		close(fd);

		if (recv_info.poff_info.result_spo){
			g_plr_state.poweroff_pos = recv_info.last_poff_lsn;
			return 1;
		}

		mdelay(500);
	}
#else
	msync(g_mmc_data_buf_addr, PAGE_SIZE, MS_INVALIDATE);
	printf ("[%s] result_spo : %d\n", __func__, g_mmc_data_buf_addr->poff_info.result_spo);
	if (g_mmc_data_buf_addr->poff_info.result_spo)
		return 1;
#endif
	return 0;
}

int excute_internal_reset(void)
{
#if 1
	int ret = 0;
	int fd = 0;
	internal_info_t recv_info;

	memset (&recv_info, 0, sizeof(internal_info_t));

	fd = open(DEVICE_POWEROFF_DATA_PATH, O_RDWR|O_SYNC);

	if (fd < 0) {
		plr_err("[%s] Open internal info fail\n", __func__);
		perror("");
		return -1;
	}
	
	ret = read(fd, &recv_info, sizeof(internal_info_t));
	
	if (ret < 0){
		plr_err("[%s] Read internal info fail\n", __func__);
		perror("");
		return -1;
	}

	recv_info.poff_info.result_spo = 0;

	ret = write(fd, &recv_info, sizeof(internal_info_t));

	if (ret < 0)
	{
		plr_err("[%s] Write internal info fail\n", __func__);
		perror("");
		return -1;
	}
	close(fd);

#else
	g_mmc_data_buf_add->poff_info.result_spo = 0;
	msync(g_mmc_data_buf_addr, PAGE_SIZE, MS_INVALIDATE);
	printf ("[%s] result_spo : %d\n", __func__, g_mmc_data_buf_addr->poff_info.result_spo);
	if (g_mmc_data_buf_addr->poff_info.result_spo)
		return 1;
#endif

	return 0;
}

int send_data_poweroff(internal_info_t *internal_info)
{
	int ret = 0;
	int fd = 0;
	static internal_info_t send_info;

	if (internal_info == NULL)
		memset(&send_info, 0, sizeof(internal_info_t));
	else
		memcpy(&send_info, internal_info, sizeof(internal_info_t));
	
	fd = open(DEVICE_POWEROFF_DATA_PATH, O_RDWR|O_SYNC);

	if (fd < 0)
	{
		plr_info("[%s] Open internal info fail\n", __func__);
		perror("");
		return -1;
	}
	
	ret = write(fd, &send_info, sizeof(internal_info_t));
	
	if (ret != sizeof(internal_info_t)){
		plr_info("[%s] Send internal info fail\n", __func__);
		perror("");
		return -1;
	}
	close(fd);
	return 0;
}

int console_init_r(void)
{
	system("echo 3 > /proc/sys/vm/drop_caches");
	system("echo \"2 4 1 7\" > /proc/sys/kernel/printk");
	prepare_mmc();
	prepare_other_devices();
	prepare_buffers();

	return 0;
}

int tstc(void)
{
	return linux_kbhit();
}

void udelay(unsigned long usec)
{
	usleep(usec);
}

int run_command (const char *cmd, int flag)
{
	char buf[PLRTOKEN_BUFFER_SIZE] = {0};
	int ret;

	if (!strncmp(cmd, "date", strlen("date"))){
		snprintf(buf, PLRTOKEN_BUFFER_SIZE-1, "date %s", g_plr_state.date_info);
	}
	else if (!strncmp(cmd, "reset", strlen("reset"))){
		snprintf(buf, PLRTOKEN_BUFFER_SIZE-1, "reboot");
	}
	else if (!strncmp(cmd, "setenv ipaddr", strlen("setenv ipaddr"))){
		snprintf(buf, PLRTOKEN_BUFFER_SIZE-1,"ifconfig eth0 %s netmask %s hw ether %s",
			g_tftp_state.client_ip, g_tftp_state.netmask, g_tftp_state.mac_addr);
		ret = system(cmd);
		if (ret < 0){
			plr_err("ifconfig failed\n");
			return -1;
		}

		system("ifconfig eth0 up");

		return 0;
	}
	else if (!strncmp(cmd, "tftpboot", strlen("tftpboot"))){
		snprintf(buf, PLRTOKEN_BUFFER_SIZE-1, "busybox tftp -g -r %s -l /system/xbin/plrtest_____ %s",
			g_tftp_state.file_path, g_tftp_state.server_ip);
		ret = system(cmd);
		if (ret < 0){
			plr_err("tftp download fail : %d\n", ret);
			return -1;
		}

		return 0;
	}
	else if (!strncmp(cmd, "movi write u-boot 0 41000000", strlen("movi write u-boot 0 41000000"))){
		ret = system("mv /system/xbin/plrtest_____ /system/xbin/plrtest");
		if (ret < -1){
			plr_err("plrtest rename fail : %d\n", ret);
			return -1;
		}

		ret = system("chmod 755 /sytem/xbin/plrtest");
		if (ret < -1){
			plr_err("modify permission fail : %d\n", ret);
			return -1;
		}

		return 0;
	}
	else {
		plr_err("unknown command. cmd : %s\n", cmd);
		return -1;
	}

	ret = system(buf);

	if (ret < 0){
		plr_err("command err... cmd : %s\n", buf);
		return ret;
	}

	return 0;
}

unsigned long rtc_get_time(void)
{
	return get_rtc_time();
}
