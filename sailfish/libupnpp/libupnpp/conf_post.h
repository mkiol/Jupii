/* Copyright (C) 2006-2016 J.F.Dockes
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *   02110-1301 USA
 */
/* conf_post: manual part included by auto-generated config.h. Avoid
 * being clobbered by autoheader, and undefine some problematic
 * symbols.
 */

// Get rid of macro names which could conflict with other package's
#if defined(LIBUPNPP_NEED_PACKAGE_VERSION) && \
    !defined(LIBUPNPP_PACKAGE_VERSION_DEFINED)
#define LIBUPNPP_PACKAGE_VERSION_DEFINED
static const char *LIBUPNPP_PACKAGE_VERSION = PACKAGE_VERSION;
#endif
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_URL
#undef PACKAGE_VERSION

#define LIBUPNPP_SOURCE
