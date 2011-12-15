/*
 * Copyright (C) 2011, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 * Author: Carlos Garnacho  <carlos@lanedo.com>
 */

#ifndef __TRACKER_MINER_ENUMS_H__
#define __TRACKER_MINER_ENUMS_H__

G_BEGIN_DECLS

/**
 * TrackerDirectoryFlags:
 * @TRACKER_DIRECTORY_FLAG_NONE: No flags.
 * @TRACKER_DIRECTORY_FLAG_RECURSE: Should recurse in the directory.
 * @TRACKER_DIRECTORY_FLAG_CHECK_MTIME: Should check mtimes of items in the directory.
 * @TRACKER_DIRECTORY_FLAG_MONITOR: Should setup monitors in the items found in the directory.
 * @TRACKER_DIRECTORY_FLAG_IGNORE: Should ignore the directory contents.
 * @TRACKER_DIRECTORY_FLAG_PRESERVE: Should preserve items in the directory even if the directory gets removed.
 *
 * Flags used when adding a new directory to be indexed in the #TrackerIndexingTree.
 */
typedef enum {
	TRACKER_DIRECTORY_FLAG_NONE        = 0,
	TRACKER_DIRECTORY_FLAG_RECURSE     = 1 << 1,
	TRACKER_DIRECTORY_FLAG_CHECK_MTIME = 1 << 2,
	TRACKER_DIRECTORY_FLAG_MONITOR     = 1 << 3,
	TRACKER_DIRECTORY_FLAG_IGNORE      = 1 << 4,
	TRACKER_DIRECTORY_FLAG_PRESERVE    = 1 << 5
} TrackerDirectoryFlags;

/**
 * TrackerFilterType:
 * @TRACKER_FILTER_FILE: All files matching this filter will be filtered out.
 * @TRACKER_FILTER_DIRECTORY: All directories matching this filter will be filtered out.
 * @TRACKER_FILTER_PARENT_DIRECTORY: All files in directories matching this filter will be filtered out.
 *
 * Flags used when adding a new filter in the #TrackerIndexingTree.
 */
typedef enum {
	TRACKER_FILTER_FILE,
	TRACKER_FILTER_DIRECTORY,
	TRACKER_FILTER_PARENT_DIRECTORY
} TrackerFilterType;

/**
 * TrackerFilterPolicy:
 * @TRACKER_FILTER_POLICY_DENY: Items matching the filter will be skipped.
 * @TRACKER_FILTER_POLICY_ACCEPT: Items matching the filter will be accepted.
 *
 * Flags used when defining default filter policy in the #TrackerIndexingTree.
 */
typedef enum {
	TRACKER_FILTER_POLICY_DENY,
	TRACKER_FILTER_POLICY_ACCEPT
} TrackerFilterPolicy;

G_END_DECLS

#endif /* __TRACKER_MINER_ENUMS_H__ */
