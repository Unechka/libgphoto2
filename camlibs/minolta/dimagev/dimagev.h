#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gphoto2.h>
#include <gpio.h>

/* These are the repsonse codes. */
#define DIMAGEV_STX 0x02
#define DIMAGEV_ETX 0x03
#define DIMAGEV_EOT 0x04
#define DIMAGEV_ACK 0x06
#define DIMAGEV_NAK 0x15
#define DIMAGEV_CAN 0x18

/* These are the commands. */
#define DIMAGEV_INQUIRY "\x01"
#define DIMAGEV_DRIVE_READY "\x02"
#define DIMAGEV_GET_DIR "\x03"
#define DIMAGEV_GET_IMAGE "\x04"
#define DIMAGEV_ERASE_IMAGE "\x05"
#define DIMAGEV_ERASE_ALL "\x06"
#define DIMAGEV_GET_STATUS "\x07"
#define DIMAGEV_SET_DATA "\x08"
#define DIMAGEV_GET_DATA "\x09"
#define DIMAGEV_SHUTTER "\x0a"
#define DIMAGEV_FORMAT "\x0b"
#define DIMAGEV_DIAG "\x0c"
#define DIMAGEV_GET_THUMB "\x0d"
#define DIMAGEV_SET_IMAGE "\x0e"

#define DIMAGEV_FILENAME_FMT "dv%05i.jpg"

typedef struct {
	unsigned int length;
	unsigned char buffer[1024];
} dimagev_packet;

/* This struct represents the information returned by the DIMAGEV_INQUIRY
   command. The response packet payload is 25 bytes, in the order listed
   below. The strings are all ASCII values, e.g. "MINOLTA ". The model is
   always "L1", which was apparently the development name for the Dimage V.
*/
typedef struct {
	unsigned char vendor[8];
	unsigned char model[8];
	unsigned char hardware_rev[4];
	unsigned char firmware_rev[4];
	unsigned char have_storage;
} dimagev_info_t;

/* This struct represents the information returned by the DIMAGEV_GET_STATUS
   command. This information is all wedged into seven bytes, as shown here:

                                     bits
      |-- 7 --|-- 6 --|-- 5 --|-- 4 --|-- 3 --|-- 2 --|-- 1 --|-- 0 --|
 byte |---------------------------------------------------------------|
   0  |                         Battery level                         |
   1  |            Number of images (most significant byte)           |
   2  |            Number of images (least significant byte)          |
   3  | Minimum number of images left to take (most significant byte) |
   4  | Minimum number of images left to take (least significant byte)|
   5  |   0   | Busy  |   0   | Flash |  Lens Status  |  Card Status  |
   6  |                   Camera and Card ID number                   |
      |---------------------------------------------------------------|

   Battery level: 0x00 == "not enough", 0xff == "enough"
   Lens status: 00 == normal, 01|10 == doesn't match flash, 11 == detached
   Card status: 00 == normal, 01 == full, 10 == write-protected, 11 == bad
   Flash: 1 == charging, 0 == ready

   I could probably have left it in one of these compacted structures, but
   memory is not so expensive as to spend that much effort shifting bits.
*/
typedef struct {
	unsigned char battery_level;
	unsigned int number_images;
	unsigned int minimum_images_can_take;
	unsigned char busy;
	unsigned char flash_charging;
	unsigned char lens_status;
	unsigned char card_status;
	unsigned char id_number;
} dimagev_status_t;

/* This struct represents the information returned by the DIMAGEV_GET_DATA
   command and sent with the DIMAGEV_SET_DATA command. It's another tightly
   packed bit field, as described below:

                                     bits
      |-- 7 --|-- 6 --|-- 5 --|-- 4 --|-- 3 --|-- 2 --|-- 1 --|-- 0 --|
 byte |---------------------------------------------------------------|
   0  | hmode |  OR   |  Date | Self  |     Flash     |Quality| Rec   |
   1  |                        Camera Clock (year)                    |
   2  |                        Camera Clock (month)                   |
   3  |                        Camera Clock (day)                     |
   4  |                        Camera Clock (hour)                    |
   5  |                       Camera Clock (minute)                   |
   6  |                       Camera Clock (second)                   |
   7  |                      Exposure correction data                 |
   8  | valid |              Camera and Card ID number                |
      |---------------------------------------------------------------|

	  hmode: "host mode" e.g. if the PC or camera is in control.
	  OR: Exposure correction data field is valid - used only for SET command.
	  Date: The date information is valid - used only for SET command.
	  Self: True if camera is in self-timer mode.
	  Flash: 00 == Auto, 01 == Force Flash, 10==No Flash, 11 == Invalid
	  Quality: Compression setting, 1 == Fine, 0 == Standard
	  Record mode: When setting, 1 == turn on LCD viewfinder.
	               When getting, 1 == switch in record mode.
      Camera Clock: BCD two-digit values for times. Hours are military 0-23.
	  Exposure correction data: sets exposure correction, from -127 to 128.
	  valid: Used to set "valid" for memory cards.
	  Camera and Card ID: used when switching cards.
*/
typedef struct {
	unsigned char host_mode;
	unsigned char exposure_valid;
	unsigned char date_valid;
	unsigned char self_timer_mode;
	unsigned char flash_mode;
	unsigned char quality_setting;
	unsigned char play_rec_mode;
	unsigned char year;
	unsigned char month;
	unsigned char day;
	unsigned char hour;
	unsigned char minute;
	unsigned char second;
	unsigned char exposure_correction;
	unsigned char valid;
	unsigned char id_number;
} dimagev_data_t;

/* This struct is used as the camera->camlib_data value in this library. */
typedef struct {
	int size;   
	int debug;
	gpio_device *dev;
	gpio_device_settings settings;
	dimagev_data_t *data;
	dimagev_status_t *status;
	dimagev_info_t *info;
	CameraFilesystem *fs;
} dimagev_t;

/* These functions are in packet.c */
dimagev_packet *dimagev_make_packet(unsigned char *buffer, unsigned int length, unsigned int seq);
dimagev_packet *dimagev_read_packet(dimagev_t *dimagev);
int dimagev_verify_packet(dimagev_packet *p, int debug);
dimagev_packet *dimagev_strip_packet(dimagev_packet *p);
void dimagev_dump_packet(dimagev_packet *p);
unsigned char dimagev_decimal_to_bcd(unsigned char decimal);
unsigned char dimagev_bcd_to_decimal(unsigned char bcd);
int dimagev_packet_sequence(dimagev_packet *p);

/* These functions are in data.c */
int dimagev_get_camera_data(dimagev_t *dimagev);
dimagev_data_t *dimagev_import_camera_data(unsigned char *raw_data);
unsigned char *dimagev_export_camera_data(dimagev_data_t *good_data);
void dimagev_dump_camera_data(dimagev_data_t *data);

/* These functions are in status.c */
int dimagev_get_camera_status(dimagev_t *dimagev);
dimagev_status_t *dimagev_import_camera_status(unsigned char *raw_data);
void dimagev_dump_camera_status(dimagev_status_t *status);

/* These functions are in info.c */
int dimagev_get_camera_info(dimagev_t *dimagev);
dimagev_info_t *dimagev_import_camera_info(unsigned char *raw_data);
void dimagev_dump_camera_info(dimagev_info_t *info);

/* These functions are in capture.c */
int dimagev_shutter(dimagev_t *dimagev);

/* These functions are in util.c */
int dimagev_host_mode(dimagev_t *dimagev, int newmode);
int dimagev_get_picture(dimagev_t *dimagev, int file_number, CameraFile *file);
int dimagev_delete_picture(dimagev_t *dimagev, int file_number);
int dimagev_delete_all(dimagev_t *dimagev);
