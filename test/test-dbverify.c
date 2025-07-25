/*
 * Copyright © 2012 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Red Hat
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.  Red
 * Hat makes no representations about the suitability of this software
 * for any purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THE AUTHORS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:
 *	Peter Hutterer (peter.hutterer@redhat.com)
 */

#include "config.h"

#define _GNU_SOURCE
#include <dirent.h>
#include <fcntl.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "libwacom.h"

static WacomDeviceDatabase *db_old;
static WacomDeviceDatabase *db_new;

static void
rmtmpdir(const char *tmpdir)
{
	DIR *dir;
	struct dirent *file;

	dir = opendir(tmpdir);
	if (!dir)
		return;

	while ((file = readdir(dir))) {
		g_autofree char *path = NULL;

		if (file->d_name[0] == '.')
			continue;

		g_assert(asprintf(&path, "%s/%s", tmpdir, file->d_name) != -1);
		g_assert(path);
		g_assert(remove(path) != -1);
	}

	closedir(dir);

	g_assert(remove(tmpdir) != -1);
}

static void
find_matching(gconstpointer data)
{
	g_autofree WacomDevice **devs_old, **devs_new;
	WacomDevice **devices, **d;
	WacomDevice *other;
	gboolean found = FALSE;
	int index = GPOINTER_TO_INT(data);

	devs_old = libwacom_list_devices_from_database(db_old, NULL);
	devs_new = libwacom_list_devices_from_database(db_new, NULL);

	/* Make sure each device in old has a device in new */
	devices = devs_old;
	other = devs_new[index];
	for (d = devices; *d; d++) {
		/* Two different contexts, they cannot be the same pointer */
		g_assert(other != *d);
		if (libwacom_compare(other, *d, WCOMPARE_MATCHES) == 0) {
			found = TRUE;
			break;
		}
	}
	g_assert_true(found);

	/* Make sure each device in new has a device in old */
	devices = devs_new;
	other = devs_old[index];
	found = FALSE;
	for (d = devices; *d; d++) {
		/* devices with multiple matches will have multiple
		 * devices in the list */
		if (libwacom_compare(other, *d, WCOMPARE_MATCHES) == 0) {
			found = TRUE;
			break;
		}
	}
	g_assert_true(found);
}

static void
test_database_size(void)
{
	int sz1, sz2;
	g_autofree WacomDevice **d1, **d2;

	d1 = libwacom_list_devices_from_database(db_old, NULL);
	d2 = libwacom_list_devices_from_database(db_new, NULL);
	g_assert_nonnull(d1);
	g_assert_nonnull(d2);

	sz1 = 0;
	for (WacomDevice **d = d1; *d; d++)
		sz1++;
	sz2 = 0;
	for (WacomDevice **d = d2; *d; d++)
		sz2++;
	g_assert_cmpint(sz1, ==, sz2);
}

static int
compare_databases(WacomDeviceDatabase *new)
{
	int i, rc;
	g_autofree WacomDevice **devs_new;
	WacomDevice **n;

	g_test_add_func("/dbverify/database-sizes", test_database_size);

	devs_new = libwacom_list_devices_from_database(new, NULL);

	for (n = devs_new, i = 0; *n; n++, i++) {
		char buf[1024];

		/* We need to add the test index to avoid duplicate
		   test names */
		snprintf(buf,
			 sizeof(buf),
			 "/dbverify/%03d/%04x:%04x-%s",
			 i,
			 libwacom_get_vendor_id(*n),
			 libwacom_get_product_id(*n),
			 libwacom_get_name(*n));
		g_test_add_data_func(buf, GINT_TO_POINTER(i), find_matching);
	}

	rc = g_test_run();
	return rc;
}

/* write out the current db, read it back in, compare */
static void
duplicate_database(WacomDeviceDatabase *db,
		   const char *dirname)
{
	WacomDevice **device, **devices;
	int i;

	devices = libwacom_list_devices_from_database(db, NULL);
	g_assert(devices);
	g_assert(*devices);

	for (device = devices, i = 0; *device; device++, i++) {
		int i;
		int fd;
		g_autofree char *path = NULL;
		int nstyli;
		g_autofree const WacomStylus **styli = NULL;

		g_assert(asprintf(&path,
				  "%s/%s.tablet",
				  dirname,
				  libwacom_get_match(*device)) != -1);
		g_assert(path);
		fd = open(path, O_WRONLY | O_CREAT, S_IRWXU);
		g_assert(fd >= 0);
		libwacom_print_device_description(fd, *device);
		close(fd);

		if (!libwacom_has_stylus(*device))
			continue;

		styli = libwacom_get_styli(*device, &nstyli);
		for (i = 0; i < nstyli; i++) {
			int fd_stylus;
			const WacomStylus *stylus = styli[i];
			g_autofree char *path =
				g_strdup_printf("%s/%#x.stylus",
						dirname,
						libwacom_stylus_get_id(stylus));
			fd_stylus = open(path, O_WRONLY | O_CREAT, S_IRWXU);
			g_assert(fd_stylus >= 0);
			libwacom_print_stylus_description(fd_stylus, stylus);
			close(fd_stylus);
		}
	}

	free(devices);
}

static WacomDeviceDatabase *
load_database(void)
{
	WacomDeviceDatabase *db;
	const char *datadir;

	datadir = getenv("LIBWACOM_DATA_DIR");
	if (!datadir)
		datadir = TOPSRCDIR "/data";

	db = libwacom_database_new_for_path(datadir);
	if (!db)
		printf("Failed to load data from %s", datadir);

	g_assert(db);
	return db;
}

int
main(int argc,
     char **argv)
{
	WacomDeviceDatabase *db;
	g_autofree char *dirname;
	int rc;

	g_test_init(&argc, &argv, NULL);
	g_test_set_nonfatal_assertions();

	db = load_database();

	dirname = g_dir_make_tmp("tmp.dbverify.XXXXXX", NULL);
	g_assert(dirname);

	duplicate_database(db, dirname);
	db_new = libwacom_database_new_for_path(dirname);
	g_assert(db_new);

	db_old = db;
	rc = compare_databases(db_new);
	libwacom_database_destroy(db_new);
	libwacom_database_destroy(db_old);

	rmtmpdir(dirname);

	return rc;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
