/* sierra.c:
 *
 * Copyright (C) 2001 Lutz M�ller <urc8@rz.uni-karlsruhe.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <config.h>
#include "sierra.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>


#include <gphoto2-library.h>
#include <gphoto2-port-log.h>
#include <gphoto2-debug.h>

#include "library.h"

#define TIMEOUT	   2000

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#define CHECK_STOP(camera,result) {int res; res = result; if (res < 0) {camera_stop (camera, context); gp_log (GP_LOG_DEBUG, "sierra", "Operation failed!"); return (res);}}

#define CHECK_STOP_FREE(camera,result) {int res; res = result; if (res < 0) {camera_stop (camera, context); free (camera->pl); camera->pl = NULL; return (res);}}

#define CHECK_FREE(camera,result) {int res; res = result; if (res < 0) {free (camera->pl); camera->pl = NULL; return (res);}}

int camera_start(Camera *camera, GPContext *context);
int camera_stop(Camera *camera, GPContext *context);
int get_jpeg_data(const char *data, int data_size, char **jpeg_data, int *jpeg_size);

/* Useful markers */
const char JPEG_SOI_MARKER[]  = { (char)0xFF, (char)0xD8, '\0' };
const char JPEG_SOF_MARKER[]  = { (char)0xFF, (char)0xD9, '\0' };
const char JPEG_APP1_MARKER[] = { (char)0xFF, (char)0xE1, '\0' };
const char TIFF_SOI_MARKER[]  = { (char)0x49, (char)0x49, (char)0x2A, (char)0x00, (char)0x08, '\0' };

static SierraCamera sierra_cameras[] = {
	/* Camera Model, Sierra Model, USB(vendor id, product id,
	   USB wrapper protocol) */ 
	{"Agfa ePhoto 307", 	SIERRA_MODEL_DEFAULT, 	0, 0, 0 },
	{"Agfa ePhoto 780", 	SIERRA_MODEL_DEFAULT, 	0, 0, 0 },
	{"Agfa ePhoto 780C", 	SIERRA_MODEL_DEFAULT,	0, 0, 0 },
	{"Agfa ePhoto 1280", 	SIERRA_MODEL_DEFAULT,	0, 0, 0 },
	{"Agfa ePhoto 1680", 	SIERRA_MODEL_DEFAULT,	0, 0, 0 },
	{"Apple QuickTake 200", SIERRA_MODEL_DEFAULT,	0, 0, 0 },
	{"Chinon ES-1000", 	SIERRA_MODEL_DEFAULT,	0, 0, 0 },
	{"Epson PhotoPC 500", 	SIERRA_MODEL_EPSON,	0, 0, 0 },
	{"Epson PhotoPC 550", 	SIERRA_MODEL_EPSON,	0, 0, 0 },
	{"Epson PhotoPC 600", 	SIERRA_MODEL_EPSON,	0, 0, 0 },
	{"Epson PhotoPC 700", 	SIERRA_MODEL_EPSON,	0, 0, 0 },
	{"Epson PhotoPC 800", 	SIERRA_MODEL_EPSON,	0, 0, 0 },
	{"Epson PhotoPC 3000z", SIERRA_MODEL_EPSON,	0x4b8, 0x403, 0},
	{"Nikon CoolPix 100", 	SIERRA_MODEL_DEFAULT,	0, 0, 0 },
	{"Nikon CoolPix 300", 	SIERRA_MODEL_DEFAULT,	0, 0, 0 },
	{"Nikon CoolPix 700", 	SIERRA_MODEL_DEFAULT,	0, 0, 0 },
	{"Nikon CoolPix 800", 	SIERRA_MODEL_DEFAULT,	0, 0, 0 },
        {"Nikon CoolPix 880",	SIERRA_MODEL_DEFAULT,	0x04b0, 0x0103, 0},
        {"Nikon CoolPix 900", 	SIERRA_MODEL_DEFAULT,	0, 0, 0 },
	{"Nikon CoolPix 900S", 	SIERRA_MODEL_DEFAULT,	0, 0, 0 },
	{"Nikon CoolPix 910", 	SIERRA_MODEL_DEFAULT,	0, 0, 0 },
	{"Nikon CoolPix 950", 	SIERRA_MODEL_DEFAULT,	0, 0, 0 },
	{"Nikon CoolPix 950S", 	SIERRA_MODEL_DEFAULT,	0, 0, 0 },
	{"Nikon CoolPix 990",	SIERRA_MODEL_DEFAULT,	0x04b0, 0x0102, 0},
	{"Olympus D-100Z", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus D-200L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus D-220L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus D-300L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus D-320L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus D-330R", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus D-340L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus D-340R", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus D-360L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus D-400L Zoom", SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus D-450Z", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus D-460Z", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus D-500L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus D-600L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus D-600XL", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus D-620L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus C-400", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus C-400L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus C-410", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus C-410L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus C-420", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus C-420L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus C-800", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus C-800L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus C-820", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus C-820L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus C-830L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus C-840L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus C-900 Zoom", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus C-900L Zoom", SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus C-1000L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus C-1400L", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus C-1400XL", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus C-2000Z", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
	{"Olympus C-2020Z",	SIERRA_MODEL_OLYMPUS,	0, 0, 0 },
        {"Olympus C-2040Z", 	SIERRA_MODEL_OLYMPUS,	0x07b4, 0x105, 1},
	{"Olympus C-2100UZ",    SIERRA_MODEL_OLYMPUS,	0x07b4, 0x100, 0},
	{"Olympus C-2500L",     SIERRA_MODEL_OLYMPUS,   0, 0, 0 },
	{"Olympus C-2500Z", 	SIERRA_MODEL_OLYMPUS,	0, 0, 0 }, /* Does this model exist? */
	{"Olympus C-3000Z", 	SIERRA_MODEL_OLYMPUS,	0x07b4, 0x100, 0},
	{"Olympus C-3030Z", 	SIERRA_MODEL_OLYMPUS,	0x07b4, 0x100, 0},
	{"Panasonic Coolshot NV-DCF5E", SIERRA_MODEL_DEFAULT,	0, 0, 0 },
	{"Polaroid PDC 640", 	SIERRA_MODEL_DEFAULT,	0, 0, 0 },
	{"Sanyo DSC-X300", 	SIERRA_MODEL_DEFAULT,	0, 0, 0 },
	{"Sanyo DSC-X350", 	SIERRA_MODEL_DEFAULT,	0, 0, 0 },
	{"Sanyo VPC-G200", 	SIERRA_MODEL_DEFAULT,	0, 0, 0 },
	{"Sanyo VPC-G200EX", 	SIERRA_MODEL_DEFAULT,	0, 0, 0 },
	{"Sanyo VPC-G210", 	SIERRA_MODEL_DEFAULT,	0, 0, 0 },
	{"Sanyo VPC-G250", 	SIERRA_MODEL_DEFAULT,	0, 0, 0 },
	{"Sierra Imaging SD640",SIERRA_MODEL_DEFAULT,	0, 0, 0 },
	{"", 			SIERRA_MODEL_DEFAULT,	0, 0, 0 }
};


int camera_id (CameraText *id) 
{
	strcpy(id->text, "sierra");

	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list) 
{
	int x;
	CameraAbilities a;

	for (x = 0; strlen (sierra_cameras[x].model) > 0; x++) {
		memset(&a, 0, sizeof(a));
		strcpy (a.model, sierra_cameras[x].model);
		a.status = GP_DRIVER_STATUS_PRODUCTION;
		a.port     = GP_PORT_SERIAL;
		if ((sierra_cameras[x].usb_vendor  > 0) &&
		    (sierra_cameras[x].usb_product > 0))
			a.port |= GP_PORT_USB;
		a.speed[0] = 9600;
		a.speed[1] = 19200;
		a.speed[2] = 38400;
		a.speed[3] = 57600;
		a.speed[4] = 115200;
		a.speed[5] = 0;
		a.operations        = 	GP_OPERATION_CAPTURE_IMAGE |
					GP_OPERATION_CAPTURE_PREVIEW |
					GP_OPERATION_CONFIG;
		a.file_operations   = 	GP_FILE_OPERATION_DELETE | 
					GP_FILE_OPERATION_PREVIEW |
					GP_FILE_OPERATION_AUDIO;
		a.folder_operations = 	GP_FOLDER_OPERATION_DELETE_ALL |
					GP_FOLDER_OPERATION_PUT_FILE;
		a.usb_vendor  = sierra_cameras[x].usb_vendor;
		a.usb_product = sierra_cameras[x].usb_product;
		gp_abilities_list_append (list, a);
	}

	return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
	       void *data, GPContext *context)
{
	Camera *camera = data;

	CHECK (camera_start (camera, context));
	CHECK_STOP (camera, sierra_list_files (camera, folder, list, context));
	return (camera_stop (camera, context));
}

static int
folder_list_func (CameraFilesystem *fs, const char *folder,
		      CameraList *list, void *data, GPContext *context)
{
	Camera *camera = data;

	CHECK (camera_start (camera, context));
	CHECK_STOP (camera, sierra_list_folders (camera, folder, list, context));
	return (camera_stop (camera, context));
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileInfo *info, void *data, GPContext *context)
{
	Camera *camera = data;
	int n;
	SierraPicInfo pic_info;

	/*
	 * Get the file number from the CameraFilesystem. We need numbers
	 * starting with 1.
	 */
	CHECK (n = gp_filesystem_number (camera->fs, folder, filename,
					 context));
	n++;

	info->file.fields    = GP_FILE_INFO_NONE;
	info->preview.fields = GP_FILE_INFO_NONE;
	info->audio.fields   = GP_FILE_INFO_NONE;

	/* Name of image */
	strncpy (info->file.name, filename, sizeof (info->file.name) - 1);
	info->file.name[sizeof (info->file.name) - 1] = '\0';
	info->file.fields |= GP_FILE_INFO_NAME;

	/* Get information about this image */
	CHECK (camera_start (camera, context));
	CHECK_STOP (camera, sierra_change_folder (camera, folder, context));
	CHECK_STOP (camera, sierra_get_pic_info (camera, n, &pic_info, context));

	/* Size of file */
	info->file.fields |= GP_FILE_INFO_SIZE;
	info->file.size = pic_info.size_file;

	/* Size of preview */
	info->preview.fields |= GP_FILE_INFO_SIZE;
	info->preview.size = pic_info.size_preview;

	/* Audio data? */
	if (pic_info.size_audio) {

		/* Size */
		info->audio.size = pic_info.size_audio;
		info->audio.fields |= GP_FILE_INFO_SIZE;

		/* Type */
		strcpy (info->audio.type, GP_MIME_WAV);
		info->audio.fields |= GP_FILE_INFO_TYPE;
	}

	/* Type of image and preview? */
	if (strstr (filename, ".MOV") != NULL) {
		strcpy (info->file.type, GP_MIME_QUICKTIME);
		strcpy (info->preview.type, GP_MIME_JPEG);
	} else if (strstr (filename, ".TIF") != NULL) {
		strcpy (info->file.type, GP_MIME_TIFF);
		strcpy (info->preview.type, GP_MIME_TIFF);
	} else {
		strcpy (info->file.type, GP_MIME_JPEG);
		strcpy (info->preview.type, GP_MIME_JPEG);
	}
	info->file.fields    |= GP_FILE_INFO_TYPE;
	info->preview.fields |= GP_FILE_INFO_TYPE;

	/* Image protected? */
	info->file.fields |= GP_FILE_INFO_PERMISSIONS;
	info->file.permissions = GP_FILE_PERM_READ;
	if (pic_info.locked == SIERRA_LOCKED_NO)
		info->file.permissions |= GP_FILE_PERM_DELETE;

	return (camera_stop (camera, context));
}

static int
set_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileInfo info, void *data, GPContext *context)
{
	Camera *camera = data;
	int n;
	SierraPicInfo pic_info;

	CHECK (n = gp_filesystem_number (camera->fs, folder, filename, context));
	n++;

	CHECK (camera_start (camera, context));
	CHECK_STOP (camera, sierra_change_folder (camera, folder, context));
	CHECK_STOP (camera, sierra_get_pic_info (camera, n, &pic_info, context));

	if (info.file.fields & GP_FILE_INFO_PERMISSIONS) {
		if (info.file.permissions & GP_FILE_PERM_DELETE) {
			if (pic_info.locked == SIERRA_LOCKED_YES) {
				CHECK_STOP (camera, sierra_set_locked (camera,
						n, SIERRA_LOCKED_NO, context));
			}
		} else {
			if (pic_info.locked == SIERRA_LOCKED_NO) {
				CHECK_STOP (camera, sierra_set_locked (camera,
						n, SIERRA_LOCKED_YES, context));
			}
		}
	}

	return (camera_stop (camera, context));
}

int camera_start (Camera *camera, GPContext *context)
{
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** camera_start");

	/*
	 * Opening and closing of the port happens in libgphoto2. All we
	 * do here is resetting the speed and timeout.
	 */
	switch (camera->port->type) {
	case GP_PORT_SERIAL:
		CHECK_STOP (camera, sierra_set_speed (camera,
						      camera->pl->speed, context));
		return (GP_OK);
	case GP_PORT_USB:
		CHECK_STOP (camera, gp_port_set_timeout (camera->port, 5000));
		return (GP_OK);
	default:
		return (GP_OK);
	}
}

int camera_stop (Camera *camera, GPContext *context) 
{
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** camera_stop");

	/*
	 * Closing the port happens in libgphoto2. All we do here is
	 * setting the default speed.
	 */
	switch (camera->port->type) {
	case GP_PORT_SERIAL:
		CHECK (sierra_set_speed (camera, -1, context));
		break;
	default:
		break;
	}

	return (GP_OK);
}

static int
camera_exit (Camera *camera, GPContext *context) 
{
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** camera_exit");

	if (camera->pl) {
		free (camera->pl);
		camera->pl = NULL;
	}

	return (GP_OK);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *user_data,
	       GPContext *context)
{
	Camera *camera = user_data;
        int regd, n;
	char *jpeg_data = NULL;
	int jpeg_size;
	const char *data, *mime_type;
	long int size;
	SierraPicInfo info;

	/*
	 * Get the file number from the CameraFileSystem.
	 * We need file numbers starting with 1.
	 */
	CHECK (n = gp_filesystem_number (camera->fs, folder, filename, context));
	n++;

	/* In which register do we have to look for data? */
	switch (type) {
	case GP_FILE_TYPE_PREVIEW:
		regd = 15;
		break;
	case GP_FILE_TYPE_NORMAL:
		regd = 14;
		break;
	case GP_FILE_TYPE_AUDIO:
		regd = 44;
		break;
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}

	/* Set the working folder */
	CHECK (camera_start (camera, context));
	CHECK_STOP (camera, sierra_change_folder (camera, folder, context));

	/* We need the file size in order to display progress information */
	CHECK_STOP (camera, sierra_get_pic_info (camera, n, &info, context));

	/* Get the file */
	CHECK_STOP (camera, sierra_get_string_register (camera, regd, n,
					file, NULL, &(info.size_file), context));
	CHECK (camera_stop (camera, context));

	/* Now get the data and do some post-processing */
	CHECK (gp_file_get_data_and_size (file, &data, &size));
	switch (type) {
	case GP_FILE_TYPE_PREVIEW:

		/* * Thumbnails are always JPEG files (as far as we know...) */
		CHECK (gp_file_set_mime_type (file, GP_MIME_JPEG));

		/* 
		 * Some camera (e.g. Epson 3000z) send the EXIF application
		 * data as thumbnail of a picture. The JPEG file need to be
		 * extracted. Equally for movies, the JPEG thumbnail is
		 * contained within a .MOV file. It needs to be extracted.
		 */
		get_jpeg_data (data, size, &jpeg_data, &jpeg_size);

		if (jpeg_data) {
			/* Append data to the output file */
			gp_file_set_data_and_size (file, jpeg_data, jpeg_size);
		} else {
			/* No valid JPEG data found */
			return GP_ERROR_CORRUPTED_DATA;
		}

		break;
	case GP_FILE_TYPE_NORMAL:

		/*
		 * Detect the mime type. If the resulting mime type is
		 * GP_MIME_RAW, manually set GP_MIME_QUICKTIME.
		 */
		CHECK (gp_file_detect_mime_type (file));
		CHECK (gp_file_get_mime_type (file, &mime_type));
		if (!strcmp (mime_type, GP_MIME_RAW))
			CHECK (gp_file_set_mime_type (file, GP_MIME_QUICKTIME));
		break;
	
	case GP_FILE_TYPE_AUDIO:
		CHECK (gp_file_set_mime_type (file, GP_MIME_WAV));
		break;
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}

	return (GP_OK);
}

static int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data,
		 GPContext *context)
{
	Camera *camera = data;
	int count;

	gp_debug_printf (GP_DEBUG_LOW, "sierra", 
			 "*** sierra_folder_delete_all");
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** folder: %s", folder);

	/* Set the working folder and delete all pictures there */
	CHECK (camera_start (camera, context));
	CHECK_STOP (camera, sierra_change_folder (camera, folder, context));
	CHECK_STOP (camera, sierra_delete_all (camera));

	/*
	 * Mick Grant <mickgr@drahthaar.clara.net> found out that his
	 * Nicon CoolPix 880 won't have deleted any picture at this point.
	 * It seems that those cameras just acknowledge the command but do
	 * nothing in the end. Therefore, we have to count the number of 
	 * pictures that are now on the camera. If there indeed are any 
	 * pictures, return an error. libgphoto2 will then try to manually
	 * delete them one-by-one.
	 */
	CHECK_STOP (camera, sierra_get_int_register (camera, 10, &count, context));
	if (count > 0)
		return (GP_ERROR);

	return (camera_stop (camera, context));
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *filename, void *data, GPContext *context) 
{
	Camera *camera = data;
	int n;

	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** sierra_file_delete");
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** folder: %s", folder);
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** filename: %s", filename);

	/* Get the file number from the CameraFilesystem */
	CHECK (n = gp_filesystem_number (camera->fs, folder, filename, context));

	/* Set the working folder and delete the file */
	CHECK (camera_start (camera, context));
	CHECK_STOP (camera, sierra_change_folder (camera, folder, context));
	CHECK_STOP (camera, sierra_delete (camera, n + 1, context));
	CHECK (camera_stop (camera, context));

	return (GP_OK);
}

static int
camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
		GPContext *context) 
{
	CHECK (camera_start (camera, context));
	CHECK_STOP (camera, sierra_capture (camera, type, path, context));
	CHECK (camera_stop (camera, context));

	return (GP_OK);
}

static int
camera_capture_preview (Camera *camera, CameraFile *file, GPContext *context)
{
	CHECK (camera_start (camera, context));
	CHECK_STOP (camera, sierra_capture_preview (camera, file, context));
	CHECK (camera_stop (camera, context));

	return (GP_OK);
}

static int
put_file_func (CameraFilesystem * fs, const char *folder, CameraFile * file, void *data, GPContext *context)
{
	Camera *camera = data;
	char *picture_folder;
	int ret;
	const char *data_file;
	long data_size;
	const char *filename;
	int available_memory;

	gp_file_get_name(file, &filename);
	

	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** put_file_func");
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** folder: %s", folder);
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** filename: %s", filename);
	
	/* Check the size */
	CHECK (gp_file_get_data_and_size (file, &data_file, &data_size));
	if ( data_size == 0 ) {
		gp_context_error (context,
			_("The file to be uploaded has a null length"));
		return GP_ERROR_BAD_PARAMETERS;
	}

	/* Initialize the camera */
	CHECK (camera_start (camera, context));

	/* Check the battery capacity */
	CHECK (sierra_check_battery_capacity (camera, context));

	/* Check the available memory */
	CHECK (sierra_get_memory_left(camera, &available_memory, context));
	if (available_memory < data_size) {
		gp_context_error (context,
				    _("Not enough memory available on the memory card"));
		return GP_ERROR_NO_MEMORY;
	}

	/* Get the name of the folder containing the pictures */
	if ( (ret = sierra_get_picture_folder(camera, &picture_folder)) != GP_OK ) {
		gp_context_error (context,
				    _("Cannot retrieve the name of the folder containing the pictures"));
		return ret;
	}

	/* Check the destination folder is the folder containing the pictures.
	   Otherwise, the upload is not supported by the camera. */
	if ( strcmp(folder, picture_folder) ) {
		gp_context_error (context, _("Upload is supported into the '%s' folder only"),
				    picture_folder);
		free(picture_folder);
		return GP_ERROR_NOT_SUPPORTED;
	}
	free(picture_folder);

	/* It is not required to set the destination folder: the command
	   will be ignored by the camera and the uploaded file will be put
	   into the picure folder */

	/* Upload the file */
	CHECK_STOP (camera, sierra_upload_file (camera, file, context));

	return (camera_stop (camera, context));
}

// FIXME: Is this function still usefull ?
static void dump_register (Camera *camera, GPContext *context)
{
	int ret, value, i;
	const char *description[] = {
		"?",				//   0
		"resolution",
		"date",
		"shutter speed",
		"current image number",
		"aperture",
		"color mode",
		"flash mode",
		"?",
		"?",
		"frames taken",			//  10
		"frames left",
		"size of current image",
		"size of thumbnail of current image",
		"current preview (captured)",
		"?",
		"battery life",
		"?",
		"?",
		"brightness/contrast",
		"white balance",		//  20
		"?",
		"camera id",
		"auto off (host)",
		"auto off (field)",
		"serial number",
		"software revision",
		"?",
		"memory left",
		"?",
		"?",				//  30
		"?",
		"?",
		"lens mode",
		"?",
		"lcd brightness",
		"?",
		"?",
		"lcd auto off",
		"?",
		"?",				//  40
		"?", "?", "?", "?", "?", "?", "?", "?", "?",
		"?",				//  50
		"?", "?", "?", "?", "?", "?", "?", "?", "?",
		"?",				//  60
		"?", "?", "?", "?", "?", "?", "?", "?", "?",
		"spot metering mode",		//  70
		"?",
		"zoom",
		"?", "?", "?", "?", "?", "?", 
		"current filename",
		"?",				//  80
		"?", "?",
		"number of folders in current folder/folder number",
		"current folder name",
		"?", "?", "?", "?", "?",
		"?",				//  90
		"?", "?", "?", "?", "?", "?", "?", "?", "?",
		"?", 				// 100
		"?", "?", "?", "?", "?", "?", "?", "?", "?",
		"?", 				// 110
		"?", "?", "?", "?", "?", "?", "?", "?", "?",
		"?",				// 120
		"?", "?", "?", "?", "?", "?", "?", "?", "?",
		"?"				// 130
	};
		

	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** Register:");
	for (i = 0; i < 128; i++) {
		ret = sierra_get_int_register (camera, i, &value, context);
		if (ret == GP_OK)
			gp_debug_printf (GP_DEBUG_LOW, "sierra", 
					 "***  %3i: %12i (%s)", i, value, 
					 description [i]);
	}
}

static int
camera_get_config_olympus (Camera *camera, CameraWidget **window, GPContext *context)
{
	CameraWidget *child;
	CameraWidget *section;
	char t[1024];
	int ret, value;

	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** camera_get_config_olympus");

	CHECK (camera_start (camera, context));

	gp_widget_new (GP_WIDGET_WINDOW, _("Camera Configuration"), window);
	gp_widget_new (GP_WIDGET_SECTION, _("Picture Settings"), &section);
	gp_widget_append (*window, section);

	/* Resolution */
	ret = sierra_get_int_register (camera, 1, &value, context);
        if (ret == GP_OK) {

		gp_widget_new (GP_WIDGET_RADIO, _("Resolution"), &child);
		gp_widget_add_choice (child, _("Auto"));
		gp_widget_add_choice (child, _("Standard"));
		gp_widget_add_choice (child, _("High"));
		gp_widget_add_choice (child, _("Best"));
		
                switch (value) {
		case 0: strcpy (t, _("Auto"));
			break;
                case 1: strcpy (t, _("Standard"));
                        break;
                case 2: strcpy (t, _("High"));
                        break;
                case 3: strcpy (t, _("Best"));
                        break;
                default:
                	sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* Shutter Speed */
        ret = sierra_get_int_register (camera, 3, &value, context);
        if (ret == GP_OK) {
		
		gp_widget_new (GP_WIDGET_RANGE, 
			       _("Shutter Speed (microseconds)"), 
			       &child);
		gp_widget_set_range (child, 0, 2000, 1);
		gp_widget_set_value (child, &value);
		gp_widget_append (section, child);
        }

	/* Aperture */
        ret = sierra_get_int_register (camera, 5, &value, context);
        if (ret == GP_OK) {

		gp_widget_new (GP_WIDGET_RADIO, _("Aperture"), &child);
		gp_widget_add_choice (child, _("Auto"));
		gp_widget_add_choice (child, _("Low"));
		gp_widget_add_choice (child, _("Medium"));
		gp_widget_add_choice (child, _("High"));

                switch (value) {
		case 0: strcpy (t, _("Auto"));
			break;
                case 1: strcpy (t, _("Low"));
                        break;
                case 2: strcpy (t, _("Medium"));
                        break;
                case 3: strcpy (t, _("High"));
                        break;
                default:
                        sprintf(t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* Color Mode */
        ret = sierra_get_int_register (camera, 6, &value, context);
        if (ret == GP_OK) {

		// Those values are for a C-2020 Z. If your model differs, we
		// have to distinguish models here...
		gp_widget_new (GP_WIDGET_RADIO, _("Color Mode"), &child);
		gp_widget_add_choice (child, _("Normal"));
		gp_widget_add_choice (child, _("Black/White"));
		gp_widget_add_choice (child, _("Sepia"));
		gp_widget_add_choice (child, _("White Board"));
		gp_widget_add_choice (child, _("Black Board"));

                switch (value) {
		case 0: strcpy (t, _("Normal"));
			break;
                case 1: strcpy (t, _("Black/White"));
                        break;
                case 2: strcpy (t, _("Sepia"));
                        break;
		case 3: strcpy (t, _("White Board"));
			break;
		case 4: strcpy (t, _("Black Board"));
			break;
                default:
                        sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* Flash Mode */
	ret = sierra_get_int_register (camera, 7, &value, context);
        if (ret == GP_OK) {

		gp_widget_new (GP_WIDGET_RADIO, _("Flash Mode"), &child);
		gp_widget_add_choice (child, _("Auto"));
		gp_widget_add_choice (child, _("Force"));
		gp_widget_add_choice (child, _("Off"));
		gp_widget_add_choice (child, _("Red-eye Reduction"));
		gp_widget_add_choice (child, _("Slow Sync"));

                switch (value) {
                case 0: strcpy (t, _("Auto"));
                        break;
                case 1: strcpy (t, _("Force"));
                        break;
                case 2: strcpy (t, _("Off"));
                        break;
                case 3: strcpy (t, _("Red-eye Reduction"));
                        break;
                case 4: strcpy (t, _("Slow Sync"));
                        break;
                default:
                        sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* Brightness/Contrast */
        ret = sierra_get_int_register (camera, 19, &value, context);
        if (ret == GP_OK) {

		gp_widget_new (GP_WIDGET_RADIO, _("Brightness/Contrast"), 
			       &child);
		gp_widget_add_choice (child, _("Normal"));
		gp_widget_add_choice (child, _("Bright+"));
		gp_widget_add_choice (child, _("Bright-"));
		gp_widget_add_choice (child, _("Contrast+"));
		gp_widget_add_choice (child, _("Contrast-"));

                switch (value) {
                case 0: strcpy (t, _("Normal"));
                        break;
                case 1: strcpy (t, _("Bright+"));
                        break;
                case 2: strcpy (t, _("Bright-"));
                        break;
                case 3: strcpy (t, _("Contrast+"));
                        break;
                case 4: strcpy (t, _("Contrast-"));
                        break;
                default:
                        sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* White Balance */
        ret = sierra_get_int_register (camera, 20, &value, context);
        if (ret == GP_OK) {
		
		gp_widget_new (GP_WIDGET_RADIO, _("White Balance"), &child);
		gp_widget_add_choice (child, _("Auto"));
		gp_widget_add_choice (child, _("Skylight"));
		gp_widget_add_choice (child, _("Fluorescent"));
		gp_widget_add_choice (child, _("Tungsten"));
		gp_widget_add_choice (child, _("Cloudy"));

                switch (value) {
                case 0: strcpy (t, _("Auto"));
                        break;
                case 1: strcpy (t, _("Skylight"));
                        break;
                case 2: strcpy (t, _("Fluorescent"));
                        break;
                case 3: strcpy (t, _("Tungsten"));
                        break;
                case 255:
                        strcpy (t, _("Cloudy"));
                        break;
                default:
                        sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* Lens Mode */
        ret = sierra_get_int_register (camera, 33, &value, context);
        if (ret == GP_OK) {

		gp_widget_new (GP_WIDGET_RADIO, _("Lens Mode"), &child);
		gp_widget_add_choice (child, _("Macro"));
		gp_widget_add_choice (child, _("Normal"));
		gp_widget_add_choice (child, _("Infinity/Fish-eye"));

                switch (value) {
                case 1: strcpy (t, _("Macro"));
                        break;
                case 2: strcpy (t, _("Normal"));
                        break;
                case 3: strcpy (t, _("Infinity/Fish-eye"));
                        break;
                default:
                        sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* Spot Metering Mode */
	ret = sierra_get_int_register (camera, 70, &value, context);
	if (ret == GP_OK) {
		
		gp_widget_new (GP_WIDGET_RADIO, _("Spot Metering Mode"), 
			       &child);
		gp_widget_add_choice (child, _("On"));
		gp_widget_add_choice (child, _("Off"));

		switch (value) {
		case 5:	strcpy (t, _("Off"));
			break;
		case 3: strcpy (t, _("On"));
			break;
		default:
			sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
		}
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
	}

	/* Zoom */
	ret = sierra_get_int_register (camera, 72, &value, context);
	if (ret == GP_OK) {

		gp_widget_new (GP_WIDGET_RADIO, _("Zoom"), &child);
		gp_widget_add_choice (child, _("1x"));
		gp_widget_add_choice (child, _("1.6x"));
		gp_widget_add_choice (child, _("2x"));
		gp_widget_add_choice (child, _("2.5x"));

		switch (value) {
		case 0: strcpy (t, _("1x"));
			break;
		case 8: strcpy (t, _("2x"));
			break;
		case 520:
			strcpy (t, _("1.6x"));
			break;
		case 1032:
			strcpy (t, _("2.5x"));
			break;
		default:
			sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
		}
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
	}

	gp_widget_new (GP_WIDGET_SECTION, _("Camera Settings"), &section);
	gp_widget_append (*window, section);

	/* Auto Off (host) */
	ret = sierra_get_int_register (camera, 23, &value, context);
	if (ret == GP_OK) {
		
		gp_widget_new (GP_WIDGET_RANGE, _("Auto Off (host) "
				       "(in seconds)"), &child);
		gp_widget_set_info (child, _("How long will it take until the "
				    "camera powers off when connected to the "
				    "computer?"));
		gp_widget_set_range (child, 0, 255, 1);
		gp_widget_set_value (child, &value);
		gp_widget_append (section, child);
	}

	/* Auto Off (field) */
	ret = sierra_get_int_register (camera, 24, &value, context);
	if (ret == GP_OK) {

		gp_widget_new (GP_WIDGET_RANGE, _("Auto Off (field) "
				       "(in seconds)"), &child);
		gp_widget_set_info (child, _("How long will it take until the "
				    "camera powers off when not connected to "
				    "the computer?"));
		gp_widget_set_range (child, 0, 255, 1);
		gp_widget_set_value (child, &value);
		gp_widget_append (section, child);
	}

	/* LCD Brightness */
	ret = sierra_get_int_register (camera, 35, &value, context);
	if (ret == GP_OK) {
		
		gp_widget_new (GP_WIDGET_RANGE, _("LCD Brightness"), &child);
		gp_widget_set_range (child, 1, 7, 1);
		gp_widget_set_value (child, &value);
		gp_widget_append (section, child);
	}

	/* LCD Auto Off */
	ret = sierra_get_int_register (camera, 38, &value, context);
	if (ret == GP_OK) {

		gp_widget_new (GP_WIDGET_RANGE, _("LCD Auto Off (in "
				       "seconds)"), &child);
		gp_widget_set_range (child, 0, 255, 1);
		gp_widget_set_value (child, &value);
		gp_widget_append (section, child);
	}

	return (camera_stop (camera, context));
}

static int
camera_set_config_olympus (Camera *camera, CameraWidget *window, GPContext *context)
{
	CameraWidget *child;
	char *value;
	int i = 0;

	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** camera_set_config");

	CHECK (camera_start (camera, context));

	/* Resolution */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting resolution");
	if ((gp_widget_get_child_by_label (window, _("Resolution"), &child)
		== GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("Auto")) == 0) {
			i = 0;
		} else if (strcmp (value, _("Standard")) == 0) {
			i = 1;
		} else if (strcmp (value, _("High")) == 0) {
			i = 2;
		} else if (strcmp (value, _("Best")) == 0) {
			i = 3;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 1, i, context));
	}
	
	/* Shutter Speed */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting shutter speed");
	if ((gp_widget_get_child_by_label (window, 
		_("Shutter Speed (microseconds)"), &child) == GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &i);
		CHECK_STOP (camera, sierra_set_int_register (camera, 3, i, context));
	}

	/* Aperture */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting aperture");
	if ((gp_widget_get_child_by_label (window, _("Aperture"), &child) 
		== GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("Auto")) == 0) {
			i = 0;
		} else if (strcmp (value, _("Low")) == 0) {
			i = 1;
		} else if (strcmp (value, _("Medium")) == 0) {
			i = 2;
		} else if (strcmp (value, _("High")) == 0) {
			i = 3;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 5, i, context));
	}

	/* Color Mode */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting color mode");
	if ((gp_widget_get_child_by_label (window, _("Color Mode"), &child)
		== GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("Normal")) == 0) {
			i = 0;
		} else if (strcmp (value, _("Black/White")) == 0) {
			i = 1;
		} else if (strcmp (value, _("Sepia")) == 0) {
			i = 2;
		} else if (strcmp (value, _("White Board")) == 0) {
			i = 3;
		} else if (strcmp (value, _("Black Board")) == 0) {
			i = 4;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 6, i, context));
	}

	/* Flash Mode */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting flash mode");
	if ((gp_widget_get_child_by_label (window, _("Flash Mode"), &child)
		== GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("Auto")) == 0) {
			i = 0;
		} else if (strcmp (value, _("Force")) == 0) {
			i = 1;
		} else if (strcmp (value, _("Off")) == 0) {
			i = 2;
		} else if (strcmp (value, _("Red-eye Reduction")) == 0) {
			i = 3;
		} else if (strcmp (value, _("Slow Sync")) == 0) {
			i = 4;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 7, i, context));
	}

	/* Brightness/Contrast */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", 
			 "*** setting brightness/contrast");
	if ((gp_widget_get_child_by_label (window, _("Brightness/Contrast"), 
		&child) == GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("Normal")) == 0) {
			i = 0;
		} else if (strcmp (value, _("Bright+")) == 0) {
			i = 1;
		} else if (strcmp (value, _("Bright-")) == 0) {
			i = 2;
		} else if (strcmp (value, _("Contrast+")) == 0) {
			i = 3;
		} else if (strcmp (value, _("Contrast-")) == 0) {
			i = 4;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 19, i, context));
	}

	/* White Balance */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting white balance");
	if ((gp_widget_get_child_by_label (window, _("White Balance"), &child)
		== GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("Auto")) == 0) {
			i = 0;
		} else if (strcmp (value, _("Skylight")) == 0) {
			i = 1;
		} else if (strcmp (value, _("Fluorescent")) == 0) {
			i = 2;
		} else if (strcmp (value, _("Tungsten")) == 0) {
			i = 3;
		} else if (strcmp (value, _("Cloudy")) == 0) {
			i = 4;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 20, i, context));
	}

	/* Lens Mode */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting lens mode");
	if ((gp_widget_get_child_by_label (window, _("Lens Mode"), &child)
		== GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
                if (strcmp (value, _("Macro")) == 0) {
			i = 1;
		} else if (strcmp (value, _("Normal")) == 0) {
			i = 2;
		} else if (strcmp (value, _("Infinity/Fish-eye")) == 0) {
			i = 3;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 33, i, context));
        }

	/* Spot Metering Mode */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", 
			 "*** setting spot metering mode");
	if ((gp_widget_get_child_by_label (window, _("Spot Metering Mode"), 
		&child) == GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("On")) == 0) {
			i = 3;
		} else if (strcmp (value, _("Off")) == 0) {
			i = 5;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 33, i, context));
	}

	/* Zoom */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting zoom");
	if ((gp_widget_get_child_by_label (window, _("Zoom"), &child) 
		== GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, "1x") == 0) {
			i = 0;
		} else if (strcmp (value, "2x") == 0) {
			i = 8;
		} else if (strcmp (value, "1.6x") == 0) {
			i = 520;
		} else if (strcmp (value, "2.5x") == 0) {
			i = 1032;
		} else 
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 72, i, context));
	}

        /* Auto Off (host) */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting auto off (host)");
	if ((gp_widget_get_child_by_label (window, _("Auto Off (host) "
					  "(in seconds)"), &child) == GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &i);
                CHECK_STOP (camera, sierra_set_int_register (camera, 23, i, context));
        }

        /* Auto Off (field) */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", 
			 "*** setting auto off (field)");
	if ((gp_widget_get_child_by_label (window, _("Auto Off (field) "
		"(in seconds)"), &child) == GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &i);
		CHECK_STOP (camera, sierra_set_int_register (camera, 24, i, context));
	}

        /* LCD Brightness */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting lcd brightness");
	if ((gp_widget_get_child_by_label (window, 
		_("LCD Brightness"), &child) == GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &i);
		CHECK_STOP (camera, sierra_set_int_register (camera, 35, i, context));
	}

        /* LCD Auto Off */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting lcd auto off");
	if ((gp_widget_get_child_by_label (window, 
		_("LCD Auto Off (in seconds)"), &child) == GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &i);
		CHECK_STOP (camera, sierra_set_int_register (camera, 38, i, context));
        }

	return (camera_stop (camera, context));
}


static int
camera_get_config_epson (Camera *camera, CameraWidget **window, GPContext *context)
{
	CameraWidget *child;
	CameraWidget *section;
	char t[1024];
	int ret, value;

	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** camera_get_config_epson");

	CHECK (camera_start (camera, context));

	gp_widget_new (GP_WIDGET_WINDOW, _("Camera Configuration"), window);

	gp_widget_new (GP_WIDGET_SECTION, _("Shot Settings"), &section);
	gp_widget_append (*window, section);

	/* Aperture */
        ret = sierra_get_int_register (camera, 5, &value, context);
        if (ret == GP_OK) {
		gp_widget_new (GP_WIDGET_RADIO, _("Aperture"), &child);
		gp_widget_add_choice (child, _("F2"));
		gp_widget_add_choice (child, _("F2.3"));
		gp_widget_add_choice (child, _("F2.8"));
		gp_widget_add_choice (child, _("F4"));
		gp_widget_add_choice (child, _("F5.6"));
		gp_widget_add_choice (child, _("F8"));
		gp_widget_add_choice (child, _("auto"));
                switch (value) {
		case 0: strcpy (t, _("F2"));
			break;
                case 1: strcpy (t, _("F2.3"));
			break;
                case 2: strcpy (t, _("F2.8"));
			break;
                case 3: strcpy (t, _("F4"));    
			break;
		case 4: strcpy (t, _("F5.6"));  
			break;
		case 5: strcpy (t, _("F8"));    
			break;
		case 6: strcpy (t, _("auto"));  
			break;
                default:
			sprintf(t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* Flash Mode */
	ret = sierra_get_int_register (camera, 7, &value, context);
        if (ret == GP_OK) {
		gp_widget_new (GP_WIDGET_RADIO, _("Flash Mode"), &child);
		gp_widget_add_choice (child, _("Auto"));
		gp_widget_add_choice (child, _("Force"));
		gp_widget_add_choice (child, _("Off"));
		gp_widget_add_choice (child, _("Red-eye Reduction"));
		gp_widget_add_choice (child, _("Slow Sync"));
                switch (value) {
                case 0: strcpy (t, _("Auto"));
                        break;
                case 1: strcpy (t, _("Force"));
                        break;
                case 2: strcpy (t, _("Off"));
                        break;
                case 3: strcpy (t, _("Red-eye Reduction"));
                        break;
                case 4: strcpy (t, _("Slow Sync"));
                        break;
                default:
                        sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* White Balance */
        ret = sierra_get_int_register (camera, 20, &value, context);
        if (ret == GP_OK) {
		gp_widget_new (GP_WIDGET_RADIO, _("White Balance"), &child);
		gp_widget_add_choice (child, _("Auto"));
		gp_widget_add_choice (child, _("Fixed"));
		gp_widget_add_choice (child, _("Custom"));

                switch (value) {
                case 0: strcpy (t, _("Auto"));
                        break;
                case 1: strcpy (t, _("Fixed"));
                        break;
                case 225: strcpy (t, _("Custom"));
                        break;
                default:
                        sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	gp_widget_new (GP_WIDGET_SECTION, _("Picture Settings"), &section);
	gp_widget_append (*window, section);

	/* Lens Mode */
        ret = sierra_get_int_register (camera, 33, &value, context);
        if (ret == GP_OK) {
		gp_widget_new (GP_WIDGET_RADIO, _("Lens Mode"), &child);
		gp_widget_add_choice (child, _("Macro"));
		gp_widget_add_choice (child, _("Normal"));
                switch (value) {
                case 1: strcpy (t, _("Macro"));
                        break;
                case 2: strcpy (t, _("Normal"));
                        break;
                default:
                        sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* Resolution */
	ret = sierra_get_int_register (camera, 1, &value, context);
        if (ret == GP_OK) {
		gp_widget_new (GP_WIDGET_RADIO, _("Resolution"), &child);
		gp_widget_add_choice (child, _("Standard"));
		gp_widget_add_choice (child, _("Fine"));
		gp_widget_add_choice (child, _("SuperFine"));
		gp_widget_add_choice (child, _("HyPict"));
		
                switch (value) {
		case 1: strcpy (t, _("Standard"));
			break;
                case 2: strcpy (t, _("Fine"));
			break;
                case 3: strcpy (t, _("SuperFine"));
			break;
                case 34: strcpy (t, _("HyPict"));
			break;
                default:
			sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* Color Mode */
	ret = sierra_get_int_register (camera, 6, &value, context);
	if (ret == GP_OK) {
		gp_widget_new (GP_WIDGET_RADIO, _("Color Mode"), &child);
		gp_widget_add_choice (child, _("color"));
		gp_widget_add_choice (child, _("black & white"));
		switch (value) {
		case 1: strcpy (t, _("color"));
			break;
		case 2: strcpy (t, _("black & white"));
			break;
		default:
			sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
		}
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
	}

	gp_widget_new (GP_WIDGET_SECTION, _("Camera Settings"), &section);
	gp_widget_append (*window, section);

	/* Auto Off (host) */
	ret = sierra_get_int_register (camera, 23, &value, context);
	if (ret == GP_OK) {
		
		gp_widget_new (GP_WIDGET_RANGE, _("Auto Off (host) "
				       "(in seconds)"), &child);
		gp_widget_set_info (child, _("How long will it take until the "
				    "camera powers off when connected to the "
				    "computer?"));
		gp_widget_set_range (child, 0, 255, 1);
		gp_widget_set_value (child, &value);
		gp_widget_append (section, child);
	}

	/* Auto Off (field) */
	ret = sierra_get_int_register (camera, 24, &value, context);
	if (ret == GP_OK) {

		gp_widget_new (GP_WIDGET_RANGE, _("Auto Off (field) "
				       "(in seconds)"), &child);
		gp_widget_set_info (child, _("How long will it take until the "
				    "camera powers off when not connected to "
				    "the computer?"));
		gp_widget_set_range (child, 0, 255, 1);
		gp_widget_set_value (child, &value);
		gp_widget_append (section, child);
	}

	/* Language */
	ret = sierra_get_int_register (camera, 53, &value, context);
        if (ret == GP_OK) {
		gp_widget_new (GP_WIDGET_RADIO, _("Language"), &child);
		gp_widget_add_choice (child, _("Korean"));
		gp_widget_add_choice (child, _("English"));
		gp_widget_add_choice (child, _("French"));
		gp_widget_add_choice (child, _("German"));
		gp_widget_add_choice (child, _("Italian"));
		gp_widget_add_choice (child, _("Japanese"));
		gp_widget_add_choice (child, _("Spanish"));
		gp_widget_add_choice (child, _("Portugese"));
                switch (value) {
                case 1: strcpy (t, _("Korean"));
                        break;
                case 3: strcpy (t, _("English"));
                        break;
                case 4: strcpy (t, _("French"));
                        break;
                case 5: strcpy (t, _("German"));
                        break;
                case 6: strcpy (t, _("Italian"));
                        break;
		case 7: strcpy (t, _("Japanese"));
			break;
		case 8: strcpy (t, _("Spanish"));
			break;
		case 9: strcpy (t, _("Portugese"));
			break;
                default:
                        sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* Date & Time */
	ret = sierra_get_int_register (camera, 2, &value, context);
	if (ret == GP_OK) {
		gp_widget_new (GP_WIDGET_DATE, _("Date & Time"), &child);
		gp_widget_set_value (child, &value);
		gp_widget_append (section, child);
	}

	return (camera_stop (camera, context));
}

static int
camera_set_config_epson (Camera *camera, CameraWidget *window, GPContext *context)
{
	CameraWidget *child;
	char *value;
	int i = 0;

	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** camera_set_config_epson");

	CHECK (camera_start (camera, context));

	/* Aperture */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting aperture");
	if ((gp_widget_get_child_by_label (window, _("Aperture"), &child) 
	     == GP_OK) && gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("F2")) == 0) {
			i = 0;
		} else if (strcmp (value, _("F2.3")) == 0) {
			i = 1;
		} else if (strcmp (value, _("F2.8")) == 0) {
			i = 2;
		} else if (strcmp (value, _("F4")) == 0) {
			i = 3;
		} else if (strcmp (value, _("F5.6")) == 0) {
			i = 4;
		} else if (strcmp (value, _("F8")) == 0) {
			i = 5;
		} else if (strcmp (value, _("auto")) == 0) {
			i = 6;			
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 5, i, context));
	}

	/* Flash Mode */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting flash mode");
	if ((gp_widget_get_child_by_label (window, _("Flash Mode"), &child)
	     == GP_OK) && gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("Auto")) == 0) {
			i = 0;
		} else if (strcmp (value, _("Force")) == 0) {
			i = 1;
		} else if (strcmp (value, _("Off")) == 0) {
			i = 2;
		} else if (strcmp (value, _("Red-eye Reduction")) == 0) {
			i = 3;
		} else if (strcmp (value, _("Slow Sync")) == 0) {
			i = 4;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 7, i, context));
	}

	/* White Balance */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting white balance");
	if ((gp_widget_get_child_by_label (window, _("White Balance"), &child)
	     == GP_OK) && gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("Auto")) == 0) {
			i = 0;
		} else if (strcmp (value, _("Fixed")) == 0) {
			i = 1;
		} else if (strcmp (value, _("Custom")) == 0) {
			i = 225;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 20, i, context));
	}

	/* Lens Mode */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting lens mode");
	if ((gp_widget_get_child_by_label (window, _("Lens Mode"), &child)
		== GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
                if (strcmp (value, _("Macro")) == 0) {
			i = 1;
		} else if (strcmp (value, _("Normal")) == 0) {
			i = 2;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 33, i, context));
        }

	/* Resolution */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting resolution");
	if ((gp_widget_get_child_by_label (window, _("Resolution"), &child)
	     == GP_OK) && gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("Standard")) == 0) {
			i = 1;
		} else if (strcmp (value, _("Fine")) == 0) {
			i = 2;
		} else if (strcmp (value, _("SuperFine")) == 0) {
			i = 3;
		} else if (strcmp (value, _("HyPict")) == 0) {
			i = 34;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 1, i, context));
	}
	
	/* Color Mode */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting color mode");
	if ((gp_widget_get_child_by_label (window, _("Color Mode"), &child)
	     == GP_OK) && gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("color")) == 0) {
			i = 1;
		} else if (strcmp (value, _("black & white")) == 0) {
			i = 2;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 6, i, context));
	}

        /* Auto Off (host) */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting auto off (host)");
	if ((gp_widget_get_child_by_label (window, _("Auto Off (host) "
					  "(in seconds)"), &child) == GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &i);
                CHECK_STOP (camera, sierra_set_int_register (camera, 23, i, context));
        }

        /* Auto Off (field) */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", 
			 "*** setting auto off (field)");
	if ((gp_widget_get_child_by_label (window, _("Auto Off (field) "
		"(in seconds)"), &child) == GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &i);
		CHECK_STOP (camera, sierra_set_int_register (camera, 24, i, context));
	}

	/* Language */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting language");
	if ((gp_widget_get_child_by_label (window, _("Language"), &child)
	     == GP_OK) && gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("Korean")) == 0) {
			i = 1;
		} else if (strcmp (value, _("English")) == 0) {
			i = 3;
		} else if (strcmp (value, _("French")) == 0) {
			i = 4;
		} else if (strcmp (value, _("German")) == 0) {
			i = 5;
		} else if (strcmp (value, _("Italian")) == 0) {
			i = 6;
		} else if (strcmp (value, _("Japanese")) == 0) {
			i = 7;
		} else if (strcmp (value, _("Spanish")) == 0) {
			i = 8;
		} else if (strcmp (value, _("Portugese")) == 0) {
			i = 9;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 53, i, context));
	}

	/* Date & Time */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting date");
	if ((gp_widget_get_child_by_label (window, _("Date & Time"), &child)
		== GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &i);
		CHECK_STOP (camera, sierra_set_int_register (camera, 2, i, context));
	}

	return (camera_stop (camera, context));
}


static int
camera_get_config_default (Camera *camera, CameraWidget **window, GPContext *context)
{
	// This really should change in the near future...
	return camera_get_config_olympus (camera, window, context);
}


static int
camera_set_config_default (Camera *camera, CameraWidget *window, GPContext *context)
{
	// This really should change in the near futur...
	return camera_set_config_olympus (camera, window, context);
}


static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context) 
{
	char buf[1024*32];
	int value, ret;
	char t[1024];

	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** camera_summary");

	CHECK (camera_start (camera, context));

	/* At least on PhotoPC 3000z, if no card is present near all retrieved
	   info are either unreadable or invalid... */
	if (sierra_get_int_register(camera, 51, &value, context) == GP_OK)
		if (value == 1) {
			strcpy (buf, _("NO MEMORY CARD PRESENT\n"));
			strcpy (summary->text, buf);
			return (camera_stop (camera, context));
		}

	strcpy(buf, "");

	/* Get all the string-related info */
	ret = sierra_get_string_register (camera, 27, 0, NULL, t, &value, context);
	if (ret == GP_OK)
		sprintf (buf, _("%sCamera Model: %s\n"), buf, t);
	
	ret = sierra_get_string_register (camera, 48, 0, NULL, t, &value, context);
	if (ret == GP_OK)
		sprintf (buf, _("%sManufacturer: %s\n"), buf, t);

	ret = sierra_get_string_register (camera, 22, 0, NULL, t, &value, context);
	if (ret == GP_OK)
		sprintf (buf, _("%sCamera ID: %s\n"), buf, t);

	ret = sierra_get_string_register (camera, 25, 0, NULL, t, &value, context);
	if (ret == GP_OK)
		sprintf (buf, _("%sSerial Number: %s\n"), buf, t);

	ret = sierra_get_string_register (camera, 26, 0, NULL, t, &value, context);
	if (ret == GP_OK)
		sprintf (buf, _("%sSoftware Rev.: %s\n"), buf, t);

	/* Get all the integer information */
	if (sierra_get_int_register(camera, 40, &value, context) == GP_OK)
		sprintf (buf, _("%sFrames Taken: %i\n"), buf, value);
	if (sierra_get_int_register(camera, 11, &value, context) == GP_OK)
		sprintf (buf, _("%sFrames Left: %i\n"), buf, value);
	if (sierra_get_int_register(camera, 16, &value, context) == GP_OK)
		sprintf (buf, _("%sBattery Life: %i\n"), buf, value);
	if (sierra_get_int_register(camera, 28, &value, context) == GP_OK)
		sprintf (buf, _("%sMemory Left: %i bytes\n"), buf, value);

	/* Get date */
	if (sierra_get_int_register (camera, 2, &value, context) == GP_OK)
		sprintf (buf, _("%sDate: %s\n"), buf, 
			 ctime ((time_t*) &value));

	strcpy (summary->text, buf);

	return (camera_stop (camera, context));
}

static int
camera_manual (Camera *camera, CameraText *manual, GPContext *context) 
{
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** camera_manual");

	switch (camera->pl->model) {
	case SIERRA_MODEL_EPSON:
		strcpy (manual->text,
			_("Some notes about Epson cameras:\n"
			  "- Some parameters are not controllable remotely:\n"
			  "  * zoom\n"
			  "  * focus\n"
			  "  * custom white balance setup\n"
			  "- Configuration has been reverse-engineered with\n"
			  "  a PhotoPC 3000z, if your camera acts differently\n"
			  "  please send a mail to <gphoto-devel@gphoto.org> (in English)\n"));
		break;
	case SIERRA_MODEL_OLYMPUS:
	default:
		strcpy (manual->text, 
			_("Some notes about Olympus cameras (and others?):\n"
			  "(1) Camera Configuration:\n"
			  "    A value of 0 will take the default one (auto).\n"
			  "(2) Olympus C-3040Z (and possibly also the C-2040Z\n"
			  "    and others) have a USB PC Control mode. In order\n"
			  "    to use this mode, the camera must be switched \n"
			  "    into 'USB PC control mode'. To get to the menu \n"
			  "    for switching modes, open the memory card access\n"
			  "    door and then press and hold both of the menu \n"
			  "    buttons until the camera control menu appears.\n"
			  "    Set it to ON."));
		break;
	}

	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context) 
{
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** camera_about");
	
	strcpy (about->text, 
		_("sierra SPARClite library\n"
		"Scott Fritzinger <scottf@unr.edu>\n"
		"Support for sierra-based digital cameras\n"
		"including Olympus, Nikon, Epson, and others.\n"
		"\n"
		"Thanks to Data Engines (www.dataengines.com)\n"
		"for the use of their Olympus C-3030Z for USB\n"
		"support implementation."));

	return (GP_OK);
}

/**
 * get_jpeg_data:
 * @data: input data table
 * @data_size: table size
 * @jpeg_data: return JPEG data table (NULL if no valid data is found).
 *             To be free by the the caller.
 * @jpeg_size: return size of the jpeg_data table.
 *
 * Extract JPEG data form the provided input data (looking for the SOI
 * and SOF markers in the input data table)
 *
 * Returns: error code (either GP_OK or GP_ERROR_CORRUPTED_DATA)
 */
int get_jpeg_data(const char *data, int data_size, char **jpeg_data, int *jpeg_size)
{
	int i, ret_status;
	const char *soi_marker = NULL, *sof_marker = NULL;

	for(i=0 ; i<data_size ; i++) {
		if (memcmp(data+i, JPEG_SOI_MARKER, 2) == 0)
			soi_marker = data + i;
		if (memcmp(data+i, JPEG_SOF_MARKER, 2) == 0)
			sof_marker = data + i;
	}
   
	if (soi_marker && sof_marker) {
		/* Valid JPEG data has been found: build the output table */
		*jpeg_size = sof_marker - soi_marker + 2;
		*jpeg_data = (char*) calloc(*jpeg_size, sizeof(char));
		memcpy(*jpeg_data, soi_marker, *jpeg_size);
		ret_status = GP_OK;
	}
	else {
		/* Cannot find valid JPEG data */
		*jpeg_size = 0;
		*jpeg_data = NULL;
		ret_status = GP_ERROR_CORRUPTED_DATA;
	}

	return ret_status;
}

int camera_init (Camera *camera, GPContext *context) 
{
        int value=0;
        int x=0, ret;
        int vendor=0, product=0, usb_wrap=0;
	GPPortSettings settings;
	CameraAbilities a;

        /* First, set up all the function pointers */
        camera->functions->exit                 = camera_exit;
        camera->functions->capture_preview      = camera_capture_preview;
        camera->functions->capture              = camera_capture;
        camera->functions->summary              = camera_summary;
        camera->functions->manual               = camera_manual;
        camera->functions->about                = camera_about;

	camera->pl = calloc (1, sizeof (CameraPrivateLibrary));
	if (!camera->pl)
		return (GP_ERROR_NO_MEMORY);

	camera->pl->model = SIERRA_MODEL_DEFAULT;
	camera->pl->first_packet = 1;
	camera->pl->usb_wrap = 0;

	/* Retrieve Camera information from name */
	gp_camera_get_abilities (camera, &a);
	for (x = 0; strlen (sierra_cameras[x].model) > 0; x++) {
		if (!strcmp (sierra_cameras[x].model, a.model)) {
			camera->pl->model = sierra_cameras[x].sierra_model;
			vendor = sierra_cameras[x].usb_product;
                        usb_wrap = sierra_cameras[x].usb_wrap;
               }
	}

	switch (camera->pl->model) {
	case SIERRA_MODEL_EPSON:
		camera->functions->get_config = camera_get_config_epson;
		camera->functions->set_config = camera_set_config_epson;
		break;
	case SIERRA_MODEL_OLYMPUS:
		camera->functions->get_config = camera_get_config_olympus;
		camera->functions->set_config = camera_set_config_olympus;
		break;
	default:
		camera->functions->get_config = camera_get_config_default;
		camera->functions->set_config = camera_set_config_default;
		break;
	}
	
	CHECK_FREE (camera, gp_port_get_settings (camera->port, &settings));
        switch (camera->port->type) {
        case GP_PORT_SERIAL:

		/* Remember the speed */
		camera->pl->speed = settings.serial.speed;

                settings.serial.speed    = 19200;
                settings.serial.bits     = 8;
                settings.serial.parity   = 0;
                settings.serial.stopbits = 1;

                break;

        case GP_PORT_USB:

                /* Test if we have usb information */
                if ((vendor == 0) && (product == 0)) {
                        free (camera->pl);
                        camera->pl = NULL;
                        return (GP_ERROR_MODEL_NOT_FOUND);
                } else {
			camera->pl->usb_wrap = usb_wrap;
		}

		/* Use the defaults the core parsed */
                break;

        default:

                free (camera->pl);
                camera->pl = NULL;
                return (GP_ERROR_UNKNOWN_PORT);
        }

        CHECK_FREE (camera, gp_port_set_settings (camera->port, settings));
        CHECK_FREE (camera, gp_port_set_timeout (camera->port, TIMEOUT));

        /* Establish a connection */
        CHECK_FREE (camera, camera_start (camera, context));

        /* FIXME??? What's that for? */
        ret = sierra_get_int_register (camera, 1, &value, context);
        if (ret != GP_OK) {
		gp_log (GP_LOG_DEBUG, "sierra", "Could not get register 1: %s",
			gp_camera_get_error (camera));
	}

        /* FIXME??? What's that for? "Resetting folder system"? */
        ret = sierra_set_int_register (camera, 83, -1, context);
        if (ret != GP_OK) {
		gp_log (GP_LOG_DEBUG, "sierra", "Could not set register 83 "
			"to -1: %s", gp_camera_get_error (camera));
	}

        CHECK_STOP_FREE (camera, gp_port_set_timeout (camera->port, 50));

        /* Folder support? */
        ret = sierra_set_string_register (camera, 84, "\\", 1, context);
        if (ret != GP_OK) {
                camera->pl->folders = 0;
                gp_debug_printf (GP_DEBUG_LOW, "sierra", 
                                 "*** folder support: no");
        } else {
		camera->pl->folders = 1;
                gp_debug_printf (GP_DEBUG_LOW, "sierra",
                                 "*** folder support: yes");
        }

        CHECK_STOP_FREE (camera, gp_port_set_timeout (camera->port, TIMEOUT));

        /* We are in "/" */
        strcpy (camera->pl->folder, "/");
        CHECK_STOP_FREE (camera, gp_filesystem_set_list_funcs (camera->fs,
				file_list_func, folder_list_func, camera));
        CHECK_STOP_FREE (camera, gp_filesystem_set_info_funcs (camera->fs,
                                get_info_func, set_info_func, camera));
	CHECK_STOP_FREE (camera, gp_filesystem_set_file_funcs (camera->fs,
				get_file_func, delete_file_func, camera));
	CHECK_STOP_FREE (camera, gp_filesystem_set_folder_funcs (camera->fs,
				put_file_func, delete_all_func, NULL, NULL, camera));

        return (camera_stop (camera, context));
}

