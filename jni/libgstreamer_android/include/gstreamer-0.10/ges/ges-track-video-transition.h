/* GStreamer Editing Services
 * Copyright (C) 2010 Brandon Lewis <brandon.lewis@collabora.co.uk>
 *               2010 Nokia Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _GES_TRACK_VIDEO_TRANSITION
#define _GES_TRACK_VIDEO_TRANSITION

#include <glib-object.h>
#include <ges/ges-types.h>
#include <ges/ges-track-transition.h>

G_BEGIN_DECLS

#define GES_TYPE_TRACK_VIDEO_TRANSITION ges_track_video_transition_get_type()

#define GES_TRACK_VIDEO_TRANSITION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GES_TYPE_TRACK_VIDEO_TRANSITION, GESTrackVideoTransition))

#define GES_TRACK_VIDEO_TRANSITION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GES_TYPE_TRACK_VIDEO_TRANSITION, GESTrackVideoTransitionClass))

#define GES_IS_TRACK_VIDEO_TRANSITION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GES_TYPE_TRACK_VIDEO_TRANSITION))

#define GES_IS_TRACK_VIDEO_TRANSITION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GES_TYPE_TRACK_VIDEO_TRANSITION))

#define GES_TRACK_VIDEO_TRANSITION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GES_TYPE_TRACK_VIDEO_TRANSITION, GESTrackVideoTransitionClass))

typedef struct _GESTrackVideoTransitionPrivate GESTrackVideoTransitionPrivate;

/**
 * GESTrackVideoTransition:
 */

struct _GESTrackVideoTransition {
  GESTrackTransition parent;

  /*< private >*/

  GESTrackVideoTransitionPrivate *priv;

  /* Padding for API extension */
  gpointer _ges_reserved[GES_PADDING];
};

/**
 * GESTrackVideoTransitionClass:
 * @parent_class: parent class
 *
 */

struct _GESTrackVideoTransitionClass {
  GESTrackTransitionClass parent_class;

  /*< private >*/
  /* Padding for API extension */
  gpointer _ges_reserved[GES_PADDING];
};

GType ges_track_video_transition_get_type               (void);
GESTrackVideoTransition* ges_track_video_transition_new (void);

gboolean ges_track_video_transition_set_transition_type (GESTrackVideoTransition * self,
                                                         GESVideoStandardTransitionType type);
GESVideoStandardTransitionType
ges_track_video_transition_get_transition_type          (GESTrackVideoTransition * trans);

void ges_track_video_transition_set_border              (GESTrackVideoTransition * self,
                                                         guint value);
gint ges_track_video_transition_get_border              (GESTrackVideoTransition * self);

void ges_track_video_transition_set_inverted            (GESTrackVideoTransition * self,
                                                         gboolean inverted);
gboolean ges_track_video_transition_is_inverted        (GESTrackVideoTransition * self);

G_END_DECLS

#endif /* _GES_TRACK_VIDEO_transition */

