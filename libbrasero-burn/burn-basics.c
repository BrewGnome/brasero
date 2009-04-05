/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-burn
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-burn is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-burn authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-burn. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-burn is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */
#include <string.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include "burn-basics.h"
#include "burn-debug.h"
#include "burn-caps.h"
#include "burn-plugin-manager.h"
#include "burn-plugin-private.h"

#include "brasero-drive.h"
#include "brasero-medium-monitor.h"

static BraseroPluginManager *plugin_manager = NULL;
static BraseroMediumMonitor *medium_manager = NULL;


GQuark
brasero_burn_quark (void)
{
	static GQuark quark = 0;

	if (!quark)
		quark = g_quark_from_static_string ("BraseroBurnError");

	return quark;
}
 
const gchar *
brasero_burn_action_to_string (BraseroBurnAction action)
{
	gchar *strings [BRASERO_BURN_ACTION_LAST] = { 	"",
							N_("Getting size"),
							N_("Creating image"),
							N_("Writing"),
							N_("Blanking"),
							N_("Creating checksum"),
							N_("Copying disc"),
							N_("Copying file"),
							N_("Analysing audio files"),
							N_("Transcoding song"),
							N_("Preparing to write"),
							N_("Writing leadin"),
							N_("Writing CD-TEXT information"),
							N_("Finalising"),
							N_("Writing leadout"),
						        N_("Starting to record"),
							N_("Success") };
	return _(strings [action]);
}

/**
 * utility functions
 */

gboolean
brasero_check_flags_for_drive (BraseroDrive *drive,
			       BraseroBurnFlag flags)
{
	BraseroMedia media;
	BraseroMedium *medium;

	if (!drive)
		return TRUE;

	if (brasero_drive_is_fake (drive))
		return TRUE;

	medium = brasero_drive_get_medium (drive);
	if (!medium)
		return TRUE;

	media = brasero_medium_get_status (medium);
	if (flags & BRASERO_BURN_FLAG_DUMMY) {
		/* This is always FALSE */
		if (media & BRASERO_MEDIUM_PLUS)
			return FALSE;

		if (media & BRASERO_MEDIUM_DVD) {
			if (!brasero_medium_can_use_dummy_for_sao (medium))
				return FALSE;
		}
		else if (flags & BRASERO_BURN_FLAG_DAO) {
			if (!brasero_medium_can_use_dummy_for_sao (medium))
				return FALSE;
		}
		else if (!brasero_medium_can_use_dummy_for_tao (medium))
			return FALSE;
	}

	if (flags & BRASERO_BURN_FLAG_BURNPROOF) {
		if (!brasero_medium_can_use_burnfree (medium))
			return FALSE;
	}

	return TRUE;
}

gchar *
brasero_string_get_localpath (const gchar *uri)
{
	gchar *localpath;
	gchar *realuri;
	GFile *file;

	if (!uri)
		return NULL;

	if (uri [0] == '/')
		return g_strdup (uri);

	if (strncmp (uri, "file://", 7))
		return NULL;

	file = g_file_new_for_commandline_arg (uri);
	realuri = g_file_get_uri (file);
	g_object_unref (file);

	localpath = g_filename_from_uri (realuri, NULL, NULL);
	g_free (realuri);

	return localpath;
}

gchar *
brasero_string_get_uri (const gchar *uri)
{
	gchar *uri_return;
	GFile *file;

	if (!uri)
		return NULL;

	if (uri [0] != '/')
		return g_strdup (uri);

	file = g_file_new_for_commandline_arg (uri);
	uri_return = g_file_get_uri (file);
	g_object_unref (file);

	return uri_return;
}

static void
brasero_caps_list_dump (void)
{
	GSList *iter;
	BraseroBurnCaps *self;

	self = brasero_burn_caps_get_default ();
	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		BraseroCaps *caps;

		caps = iter->data;
		BRASERO_BURN_LOG_WITH_TYPE (&caps->type,
					    caps->flags,
					    "Created %i links pointing to",
					    g_slist_length (caps->links));
	}

	g_object_unref (self);
}

gboolean
brasero_burn_library_start (void)
{
	BRASERO_BURN_LOG ("Initializing Brasero-%i.%i.%i",
			  BRASERO_MAJOR_VERSION,
			  BRASERO_MINOR_VERSION,
			  BRASERO_SUB);

	/* initialize the media library */
	brasero_media_library_start ();

	/* initialize all device list */
	if (!medium_manager)
		medium_manager = brasero_medium_monitor_get_default ();

	/* initialize plugins */
	brasero_burn_caps_get_default ();

	if (!plugin_manager)
		plugin_manager = brasero_plugin_manager_get_default ();

	brasero_caps_list_dump ();
	return TRUE;
}

GSList *
brasero_burn_library_get_plugins_list (void)
{
	plugin_manager = brasero_plugin_manager_get_default ();
	return brasero_plugin_manager_get_plugins_list (plugin_manager);
}

void
brasero_burn_library_stop (void)
{
	if (plugin_manager) {
		g_object_unref (plugin_manager);
		plugin_manager = NULL;
	}

	if (medium_manager) {
		g_object_unref (medium_manager);
		medium_manager = NULL;
	}

	/* close HAL connection */
//	brasero_hal_watch_destroy ();
}

gboolean
brasero_burn_library_can_checksum (void)
{
	GSList *iter;
	BraseroBurnCaps *self;

	self = brasero_burn_caps_get_default ();

	if (self->priv->tests == NULL) {
		g_object_unref (self);
		return FALSE;
	}

	for (iter = self->priv->tests; iter; iter = iter->next) {
		BraseroCapsTest *tmp;
		GSList *links;

		tmp = iter->data;
		for (links = tmp->links; links; links = links->next) {
			BraseroCapsLink *link;

			link = links->data;
			if (brasero_caps_link_active (link)) {
				g_object_unref (self);
				return TRUE;
			}
		}
	}

	g_object_unref (self);
	return FALSE;
}

/**
 * Used to test what the library can do based on the medium type.
 * Returns BRASERO_MEDIUM_WRITABLE if the disc can be written
 * and / or BRASERO_MEDIUM_REWRITABLE if the disc can be erased.
 */

BraseroMedia
brasero_burn_library_get_media_capabilities (BraseroMedia media)
{
	GSList *iter;
	GSList *links;
	BraseroMedia retval;
	BraseroBurnCaps *self;
	BraseroCaps *caps = NULL;

	self = brasero_burn_caps_get_default ();

	retval = BRASERO_MEDIUM_NONE;
	BRASERO_BURN_LOG_DISC_TYPE (media, "checking media caps for");

	/* we're only interested in DISC caps. There should be only one caps fitting */
	for (iter = self->priv->caps_list; iter; iter = iter->next) {
		caps = iter->data;
		if (caps->type.type != BRASERO_TRACK_TYPE_DISC)
			continue;

		if ((media & caps->type.subtype.media) == media)
			break;

		caps = NULL;
	}

	if (!caps) {
		g_object_unref (self);
		return BRASERO_MEDIUM_NONE;
	}

	/* check the links */
	for (links = caps->links; links; links = links->next) {
		GSList *plugins;
		gboolean active;
		BraseroCapsLink *link;

		link = links->data;

		/* this link must have at least one active plugin to be valid
		 * plugins are not sorted but in this case we don't need them
		 * to be. we just need one active if another is with a better
		 * priority all the better. */
		active = FALSE;
		for (plugins = link->plugins; plugins; plugins = plugins->next) {
			BraseroPlugin *plugin;

			plugin = plugins->data;
			if (brasero_plugin_get_active (plugin)) {
				/* this link is valid */
				active = TRUE;
				break;
			}
		}

		if (!active)
			continue;

		if (!link->caps) {
			/* means that it can be blanked */
			retval |= BRASERO_MEDIUM_REWRITABLE;
			continue;
		}

		/* means it can be written. NOTE: if this disc has already some
		 * data on it, it even means it can be appended */
		retval |= BRASERO_MEDIUM_WRITABLE;
	}

	g_object_unref (self);
	return retval;
}

